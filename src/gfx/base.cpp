#include "horizon/gfx/base.hpp"

#include <vulkan/vulkan_core.h>

#include <queue>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "horizon/core/core.hpp"
#include "horizon/core/logger.hpp"
#include "horizon/gfx/context.hpp"
#include "horizon/gfx/helper.hpp"
#include "horizon/gfx/rendergraph.hpp"
#include "horizon/gfx/types.hpp"

namespace gfx {

base_t::base_t(core::ref<core::window_t> window, core::ref<context_t> context)
    : _window(window), _context(context) {
  horizon_profile();
  _swapchain    = _context->create_swapchain(*_window);
  _command_pool = _context->create_command_pool({});
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    _commandbuffers[i] = _context->allocate_commandbuffer(
        {.handle_command_pool = _command_pool,
         .debug_name          = "commandbuffer_" + std::to_string(i)});
    _in_flight_fences[i]           = _context->create_fence({});
    _image_available_semaphores[i] = _context->create_semaphore({});
    _render_finished_semaphores[i] = _context->create_semaphore({});
  }

  gfx::config_descriptor_set_layout_t config_bindless_descriptor_set_layout{};
  config_bindless_descriptor_set_layout.debug_name =
      "bindless descriptor set layout";
  config_bindless_descriptor_set_layout.add_layout_binding(
      0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_ALL, 1000);
  config_bindless_descriptor_set_layout.add_layout_binding(
      1, VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_ALL, 1000);
  config_bindless_descriptor_set_layout.add_layout_binding(
      2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_ALL, 1000);
  _bindless_descriptor_set_layout = _context->create_descriptor_set_layout(
      config_bindless_descriptor_set_layout);

  // TODO: think of a way to "set" bindless context
  gfx::config_descriptor_set_t config_bindless_descriptor_set{};
  config_bindless_descriptor_set.debug_name = "bindless descriptor";
  config_bindless_descriptor_set.handle_descriptor_set_layout =
      _bindless_descriptor_set_layout;
  _bindless_descriptor_set =
      _context->allocate_descriptor_set(config_bindless_descriptor_set);
}

base_t::~base_t() {
  horizon_profile();
  _context->wait_idle();
  for (auto &[handle, buffer] : _buffers) {
    for (auto handle_buffer : buffer.handle_buffers) {
      _context->destroy_buffer(handle_buffer);
    }
  }
  for (auto &[handle, descriptor_set] : _descriptor_sets) {
    for (auto handle_descriptor_set : descriptor_set.handle_descriptor_sets) {
      _context->free_descriptor_set(handle_descriptor_set);
    }
  }
  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    _context->free_commandbuffer(_commandbuffers[i]);
    _context->destroy_fence(_in_flight_fences[i]);
    _context->destroy_semaphore(_image_available_semaphores[i]);
    _context->destroy_semaphore(_render_finished_semaphores[i]);
  }
  _context->destroy_descriptor_set_layout(_bindless_descriptor_set_layout);
  _context->free_descriptor_set(_bindless_descriptor_set);
  _context->destroy_swapchain(_swapchain);
  _context->destroy_command_pool(_command_pool);
}

void base_t::begin() {
  horizon_profile();
  if (_resize) {
    resize_swapchain();
    _resize = false;
  }
  handle_commandbuffer_t cbuf            = _commandbuffers[_current_frame];
  handle_fence_t         in_flight_fence = _in_flight_fences[_current_frame];
  handle_semaphore_t     image_available_semaphore =
      _image_available_semaphores[_current_frame];
  handle_semaphore_t render_finished_semaphore =
      _render_finished_semaphores[_current_frame];
  _context->wait_fence(in_flight_fence);
  auto swapchain_image = _context->get_swapchain_next_image_index(
      _swapchain, image_available_semaphore, core::null_handle);
  if (!swapchain_image) {
    _resize = true;
    return;
  }
  check(swapchain_image, "Failed to get next image");
  _next_image = *swapchain_image;
  _context->reset_fence(in_flight_fence);
  _context->begin_commandbuffer(cbuf);
}

void base_t::end() {
  horizon_profile();
  handle_commandbuffer_t cbuf            = _commandbuffers[_current_frame];
  handle_fence_t         in_flight_fence = _in_flight_fences[_current_frame];
  handle_semaphore_t     image_available_semaphore =
      _image_available_semaphores[_current_frame];
  handle_semaphore_t render_finished_semaphore =
      _render_finished_semaphores[_current_frame];
  _context->end_commandbuffer(cbuf);
  _context->submit_commandbuffer(
      cbuf, {image_available_semaphore},
      {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
      {render_finished_semaphore}, in_flight_fence);
  if (!_context->present_swapchain(_swapchain, _next_image,
                                   {render_finished_semaphore})) {
    _resize = true;
    return;
  }
  _current_frame = (_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void base_t::resize_swapchain() {
  horizon_profile();
  auto [width, height] = _window->dimensions();
  _context->wait_idle();
  _context->destroy_swapchain(_swapchain);
  _swapchain = _context->create_swapchain(*_window);
}

void base_t::begin_swapchain_renderpass() {
  horizon_profile();
  handle_commandbuffer_t cbuf = _commandbuffers[_current_frame];
  _context->cmd_image_memory_barrier(
      cbuf, _context->get_swapchain_images(_swapchain)[_next_image],
      VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
  auto rendering_attachment = swapchain_rendering_attachment(
      {0, 0, 0, 0}, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  auto [width, height] = _window->dimensions();
  _context->cmd_begin_rendering(
      cbuf, {rendering_attachment}, std::nullopt,
      VkRect2D{VkOffset2D{},
               {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}});
}

void base_t::end_swapchain_renderpass() {
  horizon_profile();
  handle_commandbuffer_t cbuf = _commandbuffers[_current_frame];
  _context->cmd_end_rendering(cbuf);
  _context->cmd_image_memory_barrier(
      cbuf, _context->get_swapchain_images(_swapchain)[_next_image],
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
}

handle_managed_buffer_t base_t::create_buffer(
    resource_update_policy_t update_policy, const config_buffer_t &config) {
  horizon_profile();
  internal::managed_buffer_t<MAX_FRAMES_IN_FLIGHT> managed_buffer{
      .update_policy = update_policy};
  if (update_policy == resource_update_policy_t::e_sparse) {
    managed_buffer.handle_buffers[0] = _context->create_buffer(config);
  } else if (update_policy == resource_update_policy_t::e_every_frame) {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
      managed_buffer.handle_buffers[i] = _context->create_buffer(config);
  }
  handle_managed_buffer_t handle =
      utils::create_and_insert_new_handle<handle_managed_buffer_t>(
          _buffers, managed_buffer);
  horizon_trace("created managed buffer");
  return handle;
}

handle_buffer_t base_t::buffer(handle_managed_buffer_t handle) {
  horizon_profile();
  internal::managed_buffer_t<MAX_FRAMES_IN_FLIGHT> &managed_buffer =
      utils::assert_and_get_data<
          internal::managed_buffer_t<MAX_FRAMES_IN_FLIGHT>>(handle, _buffers);
  if (managed_buffer.update_policy == resource_update_policy_t::e_sparse) {
    return managed_buffer.handle_buffers[0];
  } else if (managed_buffer.update_policy ==
             resource_update_policy_t::e_every_frame) {
    return managed_buffer.handle_buffers[_current_frame];
  }
  check(false, "reached unreachable");
}

handle_managed_descriptor_set_t base_t::allocate_descriptor_set(
    resource_update_policy_t       update_policy,
    const config_descriptor_set_t &config) {
  horizon_profile();
  internal::managed_descriptor_set_t<MAX_FRAMES_IN_FLIGHT>
      managed_descriptor_set{.update_policy = update_policy};
  if (update_policy == resource_update_policy_t::e_sparse) {
    managed_descriptor_set.handle_descriptor_sets[0] =
        _context->allocate_descriptor_set(config);
  } else if (managed_descriptor_set.update_policy ==
             resource_update_policy_t::e_every_frame) {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      managed_descriptor_set.handle_descriptor_sets[i] =
          _context->allocate_descriptor_set(config);
    }
  }
  handle_managed_descriptor_set_t handle =
      utils::create_and_insert_new_handle<handle_managed_descriptor_set_t>(
          _descriptor_sets, managed_descriptor_set);
  return handle;
}

handle_descriptor_set_t base_t::descriptor_set(
    handle_managed_descriptor_set_t handle) {
  horizon_profile();
  internal::managed_descriptor_set_t<MAX_FRAMES_IN_FLIGHT>
      &managed_descriptor_set = utils::assert_and_get_data<
          internal::managed_descriptor_set_t<MAX_FRAMES_IN_FLIGHT>>(
          handle, _descriptor_sets);
  if (managed_descriptor_set.update_policy ==
      resource_update_policy_t::e_sparse) {
    return managed_descriptor_set.handle_descriptor_sets[0];
  } else if (managed_descriptor_set.update_policy ==
             resource_update_policy_t::e_every_frame) {
    return managed_descriptor_set.handle_descriptor_sets[_current_frame];
  }
  check(false, "reached unreachable");
}

update_managed_descriptor_set_t<base_t::MAX_FRAMES_IN_FLIGHT>
base_t::update_managed_descriptor_set(handle_managed_descriptor_set_t handle) {
  horizon_profile();
  return {*this, handle};
}

handle_managed_timer_t base_t::create_timer(
    resource_update_policy_t update_policy, const config_timer_t &config) {
  horizon_profile();
  internal::managed_timer_t<MAX_FRAMES_IN_FLIGHT> managed_timer{
      .update_policy = update_policy};
  if (update_policy == resource_update_policy_t::e_sparse) {
    managed_timer.handle_timers[0] = _context->create_timer(config);
  } else if (update_policy == resource_update_policy_t::e_every_frame) {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
      managed_timer.handle_timers[i] = _context->create_timer(config);
  }
  handle_managed_timer_t handle =
      utils::create_and_insert_new_handle<handle_managed_timer_t>(
          _timers, managed_timer);
  horizon_trace("created managed timer");
  return handle;
}

handle_timer_t base_t::timer(handle_managed_timer_t handle) {
  horizon_profile();
  internal::managed_timer_t<MAX_FRAMES_IN_FLIGHT> &managed_timer =
      utils::assert_and_get_data<
          internal::managed_timer_t<MAX_FRAMES_IN_FLIGHT>>(handle, _timers);
  if (managed_timer.update_policy == resource_update_policy_t::e_sparse) {
    return managed_timer.handle_timers[0];
  } else if (managed_timer.update_policy ==
             resource_update_policy_t::e_every_frame) {
    return managed_timer.handle_timers[_current_frame];
  }
  check(false, "reached unreachable");
}

handle_commandbuffer_t base_t::current_commandbuffer() {
  horizon_profile();
  return _commandbuffers[_current_frame];
}

uint32_t base_t::current_frame() {
  horizon_profile();
  return _current_frame;
}

handle_image_t base_t::current_swapchain_image() {
  horizon_profile();
  return _context->get_swapchain_images(_swapchain)[_next_image];
}

handle_image_view_t base_t::current_swapchain_image_view() {
  horizon_profile();
  return _context->get_swapchain_image_views(_swapchain)[_next_image];
}

rendering_attachment_t base_t::swapchain_rendering_attachment(
    VkClearValue vk_clear_value, VkImageLayout vk_layout,
    VkAttachmentLoadOp vk_load_op, VkAttachmentStoreOp vk_store_op) {
  horizon_profile();
  rendering_attachment_t rendering_attachment{};
  rendering_attachment.clear_value  = vk_clear_value;
  rendering_attachment.image_layout = vk_layout;
  rendering_attachment.load_op      = vk_load_op;
  rendering_attachment.store_op     = vk_store_op;
  rendering_attachment.handle_image_view =
      _context->get_swapchain_image_views(_swapchain)[_next_image];
  return rendering_attachment;
}

handle_bindless_image_t base_t::new_bindless_image() {
  horizon_profile();
  handle_bindless_image_t handle = _image_counter;
  _image_counter++;
  return handle;
}

handle_bindless_sampler_t base_t::new_bindless_sampler() {
  horizon_profile();
  handle_bindless_sampler_t handle = _sampler_counter;
  _sampler_counter++;
  return handle;
}

handle_bindless_storage_image_t base_t::new_bindless_storage_image() {
  horizon_profile();
  handle_bindless_storage_image_t handle = _storage_image_counter;
  _storage_image_counter++;
  return handle;
}

void base_t::set_bindless_image(handle_bindless_image_t handle,
                                handle_image_view_t     image_view,
                                VkImageLayout           vk_image_layout) {
  horizon_profile();
  _context->update_descriptor_set(_bindless_descriptor_set)
      .push_image_write(
          0,
          {.handle_image_view = image_view, .vk_image_layout = vk_image_layout},
          static_cast<uint32_t>(handle))
      .commit();
}

void base_t::set_bindless_sampler(handle_bindless_sampler_t handle,
                                  handle_sampler_t          sampler) {
  horizon_profile();
  _context->update_descriptor_set(_bindless_descriptor_set)
      .push_image_write(1, {.handle_sampler = sampler},
                        static_cast<uint32_t>(handle))
      .commit();
}

void base_t::set_bindless_storage_image(handle_bindless_storage_image_t handle,
                                        handle_image_view_t image_view) {
  horizon_profile();
  _context->update_descriptor_set(_bindless_descriptor_set)
      .push_image_write(2,
                        {.handle_image_view = image_view,
                         .vk_image_layout   = VK_IMAGE_LAYOUT_GENERAL},
                        static_cast<uint32_t>(handle))
      .commit();
}

bool is_same_resource(const resource_t &a, const resource_t &b,
                      core::ref<context_t> context) {
  if (a.type != b.type) return false;
  if (a.type == resource_type_t::e_buffer) {
    if (a.as.buffer.buffer != b.as.buffer.buffer) return false;
    // only need size of a since a == b
    internal::buffer_t buffer     = context->get_buffer(a.as.buffer.buffer);
    uint64_t           total_size = buffer.config.vk_size;
    uint64_t           a_start    = a.as.buffer.buffer_resource_range.offset;
    uint64_t           a_size     = a.as.buffer.buffer_resource_range.size;
    uint64_t a_end = (a_size == VK_WHOLE_SIZE) ? total_size : a_start + a_size;
    uint64_t b_start = b.as.buffer.buffer_resource_range.offset;
    uint64_t b_size  = b.as.buffer.buffer_resource_range.size;
    uint64_t b_end = (b_size == VK_WHOLE_SIZE) ? total_size : b_start + b_size;
    return (a_start < b_end) && (b_start < a_end);
  }
  if (a.type == resource_type_t::e_image) {
    if (a.as.image.image != b.as.image.image) return false;
    internal::image_t image              = context->get_image(a.as.image.image);
    uint32_t          total_mip_levels   = image.config.vk_mips;
    uint32_t          total_array_layers = image.config.vk_array_layers;
    uint32_t a_mip_start = a.as.image.image_resource_range.base_mip_level;
    uint32_t a_mip_count = a.as.image.image_resource_range.level_count;
    uint32_t a_mip_end   = (a_mip_count == VK_REMAINING_MIP_LEVELS)
                               ? total_mip_levels
                               : a_mip_start + a_mip_count;
    uint32_t b_mip_start = b.as.image.image_resource_range.base_mip_level;
    uint32_t b_mip_count = b.as.image.image_resource_range.level_count;
    uint32_t b_mip_end   = (b_mip_count == VK_REMAINING_MIP_LEVELS)
                               ? total_mip_levels
                               : b_mip_start + b_mip_count;
    bool mip_overlap = (a_mip_start < b_mip_end) && (b_mip_start < a_mip_end);
    uint32_t a_layer_start = a.as.image.image_resource_range.base_array_layer;
    uint32_t a_layer_count = a.as.image.image_resource_range.layer_count;
    uint32_t a_layer_end   = (a_layer_count == VK_REMAINING_ARRAY_LAYERS)
                                 ? total_array_layers
                                 : a_layer_start + a_layer_count;
    uint32_t b_layer_start = b.as.image.image_resource_range.base_array_layer;
    uint32_t b_layer_count = b.as.image.image_resource_range.layer_count;
    uint32_t b_layer_end   = (b_layer_count == VK_REMAINING_ARRAY_LAYERS)
                                 ? total_array_layers
                                 : b_layer_start + b_layer_count;
    bool     layer_overlap =
        (a_layer_start < b_layer_end) && (b_layer_start < a_layer_end);
    return mip_overlap && layer_overlap;
  }
  throw std::runtime_error("reached unreachable");
}

bool is_pass_conflicts(const pass_t &pass_i, const pass_t &pass_j,
                       core::ref<context_t> context) {
  // RAW
  for (const auto &write_i : pass_i.write_resources) {
    for (const auto &read_j : pass_j.read_resources) {
      if (is_same_resource(write_i, read_j, context)) return true;
    }
  }
  // WAW
  for (const auto &write_i : pass_i.write_resources) {
    for (const auto &write_j : pass_j.write_resources) {
      if (is_same_resource(write_i, write_j, context)) return true;
    }
  }
  // WAR
  for (const auto &read_i : pass_i.read_resources) {
    for (const auto &write_j : pass_j.write_resources) {
      if (is_same_resource(read_i, write_j, context)) return true;
    }
  }
  return false;
}

void base_t::render_rendergraph(const rendergraph_t   &rendergraph,
                                handle_commandbuffer_t cmd) {
  std::unordered_map<uint32_t, std::set<uint32_t>> dependency_graph{};

  for (uint32_t i = 0; i < rendergraph.passes.size(); i++) {
    for (uint32_t j = i + 1; j < rendergraph.passes.size(); j++) {
      if (is_pass_conflicts(rendergraph.passes[i], rendergraph.passes[j],
                            _context))
        dependency_graph[i].emplace(j);
    }
  }

  std::unordered_map<uint32_t, uint32_t> in_degree{};
  for (uint32_t i = 0; i < rendergraph.passes.size(); i++) in_degree[i] = 0;

  for (const auto &[_, dependencies] : dependency_graph)
    for (const auto &pass_index : dependencies) in_degree[pass_index]++;

  std::queue<uint32_t> ready_queue;
  for (uint32_t i = 0; i < rendergraph.passes.size(); i++)
    if (in_degree[i] == 0) ready_queue.push(i);

  std::vector<uint32_t> order{};
  while (!ready_queue.empty()) {
    uint32_t pass_index = ready_queue.front();
    ready_queue.pop();
    order.push_back(pass_index);
    if (dependency_graph.count(pass_index)) {
      for (uint32_t dependency_index : dependency_graph.at(pass_index)) {
        in_degree[dependency_index]--;
        if (in_degree[dependency_index] == 0)
          ready_queue.push(dependency_index);
      }
    }
  }
  if (order.size() != rendergraph.passes.size())
    throw std::runtime_error(
        "Error: circular dependency detected in rendergraph");

  // using generic handle since this is an internal handle
  core::handle_t                                      resource_counter = 0;
  std::unordered_map<handle_buffer_t, core::handle_t> buffer_to_resource;
  std::unordered_map<handle_image_t, core::handle_t>  image_to_resource;
  for (const auto &pass : rendergraph.passes) {
    for (const auto &read : pass.read_resources) {
      if (read.type == resource_type_t::e_buffer) {
        if (!buffer_to_resource.contains(read.as.buffer.buffer))
          buffer_to_resource[read.as.buffer.buffer] = resource_counter++;
      } else {
        if (!image_to_resource.contains(read.as.image.image))
          image_to_resource[read.as.image.image] = resource_counter++;
      }
    }
    for (const auto &write : pass.write_resources) {
      if (write.type == resource_type_t::e_buffer) {
        if (!buffer_to_resource.contains(write.as.buffer.buffer))
          buffer_to_resource[write.as.buffer.buffer] = resource_counter++;
      } else {
        if (!image_to_resource.contains(write.as.image.image))
          image_to_resource[write.as.image.image] = resource_counter++;
      }
    }
  }

  std::unordered_map<core::handle_t, resource_state_t>
      resource_to_resource_state;

  for (auto pass_index : order) {
    const auto &pass = rendergraph.passes[pass_index];

    auto handle_resources = [&](const std::vector<resource_t> resources) {
      for (auto resource : resources) {
        if (resource.type == resource_type_t::e_buffer) {
          if (!resource_to_resource_state.contains(
                  buffer_to_resource[resource.as.buffer.buffer])) {
            check(  // sanity check
                resource.as.buffer.vk_access != VK_ACCESS_SHADER_READ_BIT,
                "ERROR: cannot read to a rendergraph managed resource without "
                "writing to it");
            _context->cmd_buffer_memory_barrier(
                cmd, resource.as.buffer.buffer, 0, resource.as.buffer.vk_access,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                resource.as.buffer.vk_pipeline_state,
                resource.as.buffer.buffer_resource_range);
            resource_state_t resource_state{};
            resource_state.type            = resource_type_t::e_buffer;
            resource_state.as.buffer_state = {
                resource.as.buffer.vk_access,
                resource.as.buffer.vk_pipeline_state};
            resource_to_resource_state
                [buffer_to_resource[resource.as.buffer.buffer]] =
                    resource_state;
          } else {
            resource_state_t &resource_state = resource_to_resource_state
                [buffer_to_resource[resource.as.buffer.buffer]];
            if (resource.as.buffer.vk_access !=
                    resource_state.as.buffer_state.vk_access ||
                (resource.as.buffer.vk_access & VK_ACCESS_SHADER_WRITE_BIT)) {
              _context->cmd_buffer_memory_barrier(
                  cmd, resource.as.buffer.buffer,
                  resource_state.as.buffer_state.vk_access,
                  resource.as.buffer.vk_access,
                  resource_state.as.buffer_state.vk_pipeline_state,
                  resource.as.buffer.vk_pipeline_state,
                  resource.as.buffer.buffer_resource_range);
            }
            resource_state.as.buffer_state = {
                resource.as.buffer.vk_access,
                resource.as.buffer.vk_pipeline_state};
          }
        } else {
          if (!resource_to_resource_state.contains(
                  image_to_resource[resource.as.image.image])) {
            _context->cmd_image_memory_barrier(
                cmd, resource.as.image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                resource.as.image.vk_image_layout, 0,
                resource.as.image.vk_access, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                resource.as.image.vk_pipeline_state,
                resource.as.image.image_resource_range);

            resource_state_t resource_state{};
            resource_state.type           = resource_type_t::e_image;
            resource_state.as.image_state = {
                resource.as.image.vk_access,
                resource.as.image.vk_pipeline_state,
                resource.as.image.vk_image_layout};
            resource_to_resource_state
                [image_to_resource[resource.as.image.image]] = resource_state;
          } else {
            resource_state_t &resource_state = resource_to_resource_state
                [image_to_resource[resource.as.image.image]];

            if (resource.as.image.vk_image_layout !=
                    resource_state.as.image_state.vk_image_layout ||
                resource.as.image.vk_access !=
                    resource_state.as.image_state.vk_access 
              // unnecessary
              /* || resource.as.image.vk_pipeline_state != 
                    resource_state.as.image_state.vk_pipeline_state */) {
              _context->cmd_image_memory_barrier(
                  cmd, resource.as.image.image,
                  resource_state.as.image_state.vk_image_layout,
                  resource.as.image.vk_image_layout,
                  resource_state.as.image_state.vk_access,
                  resource.as.image.vk_access,
                  resource_state.as.image_state.vk_pipeline_state,
                  resource.as.image.vk_pipeline_state,
                  resource.as.image.image_resource_range);
            }
            resource_state.as.image_state = {
                resource.as.image.vk_access,
                resource.as.image.vk_pipeline_state,
                resource.as.image.vk_image_layout};
          }
        }
      }
    };
    handle_resources(pass.read_resources);
    handle_resources(pass.write_resources);
    pass.callback(cmd);
  }
}

void base_t::cmd_bind_descriptor_sets(
    handle_commandbuffer_t handle_commandbuffer,
    handle_pipeline_t handle_pipeline, uint32_t vk_first_set,
    const std::vector<handle_descriptor_set_t> &handle_descriptor_sets) {
  _context->cmd_bind_descriptor_sets(handle_commandbuffer, handle_pipeline,
                                     vk_first_set, handle_descriptor_sets);
}

void base_t::cmd_bind_graphics_pipeline(
    handle_commandbuffer_t handle_commandbuffer,
    handle_pipeline_t handle_pipeline, uint32_t width, uint32_t height) {
  _context->cmd_bind_pipeline(handle_commandbuffer, handle_pipeline);
  auto [viewport, scissor] =
      gfx::helper::fill_viewport_and_scissor_structs(width, height);
  _context->cmd_set_viewport_and_scissor(handle_commandbuffer, viewport,
                                         scissor);
}

void base_t::cmd_begin_rendering(
    handle_commandbuffer_t                       handle_commandbuffer,
    const std::vector<rendering_attachment_t>   &color_rendering_attachments,
    const std::optional<rendering_attachment_t> &depth_rendering_attachment,
    const VkRect2D &vk_render_area, uint32_t vk_layer_count) {
  _context->cmd_begin_rendering(
      handle_commandbuffer, color_rendering_attachments,
      depth_rendering_attachment, vk_render_area, vk_layer_count);
}

void base_t::cmd_end_rendering(handle_commandbuffer_t handle_commandbuffer) {
  _context->cmd_end_rendering(handle_commandbuffer);
}

void base_t::cmd_draw(handle_commandbuffer_t handle_commandbuffer,
                      uint32_t vk_vertex_count, uint32_t vk_instance_count,
                      uint32_t vk_first_vertex, uint32_t vk_first_instance) {
  _context->cmd_draw(handle_commandbuffer, vk_vertex_count, vk_instance_count,
                     vk_first_vertex, vk_first_instance);
}

}  // namespace gfx

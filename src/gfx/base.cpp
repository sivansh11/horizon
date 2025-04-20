#include "horizon/gfx/base.hpp"
#include "horizon/core/core.hpp"
#include "horizon/core/logger.hpp"
#include "horizon/gfx/context.hpp"
#include "horizon/gfx/types.hpp"
#include <vulkan/vulkan_core.h>

namespace gfx {

base_t::base_t(const base_config_t &info) : _info(info) {
  horizon_profile();
  _swapchain = _info.context.create_swapchain(_info.window);
  _command_pool = _info.context.create_command_pool({});
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    _commandbuffers[i] = _info.context.allocate_commandbuffer(
        {.handle_command_pool = _command_pool,
         .debug_name = "commandbuffer_" + std::to_string(i)});
    _in_flight_fences[i] = _info.context.create_fence({});
    _image_available_semaphores[i] = _info.context.create_semaphore({});
    _render_finished_semaphores[i] = _info.context.create_semaphore({});
  }

  gfx::config_descriptor_set_layout_t config_bindless_descriptor_set_layout{};
  config_bindless_descriptor_set_layout.debug_name =
      "bindless descriptor set layout";
  config_bindless_descriptor_set_layout.add_layout_binding(
      0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_ALL, 1000);
  config_bindless_descriptor_set_layout.add_layout_binding(
      1, VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_ALL, 1000);
  _bindless_descriptor_set_layout = _info.context.create_descriptor_set_layout(
      config_bindless_descriptor_set_layout);

  // TODO: think of a way to "set" bindless context
  gfx::config_descriptor_set_t config_bindless_descriptor_set{};
  config_bindless_descriptor_set.debug_name = "bindless descriptor";
  config_bindless_descriptor_set.handle_descriptor_set_layout =
      _bindless_descriptor_set_layout;
  _bindless_descriptor_set =
      _info.context.allocate_descriptor_set(config_bindless_descriptor_set);
}

base_t::~base_t() {
  horizon_profile();
  _info.context.wait_idle();
  for (auto &[handle, buffer] : _buffers) {
    for (auto handle_buffer : buffer.handle_buffers) {
      _info.context.destroy_buffer(handle_buffer);
    }
  }
  for (auto &[handle, descriptor_set] : _descriptor_sets) {
    for (auto handle_descriptor_set : descriptor_set.handle_descriptor_sets) {
      _info.context.free_descriptor_set(handle_descriptor_set);
    }
  }
}

void base_t::begin() {
  horizon_profile();
  if (_resize) {
    resize_swapchain();
    _resize = false;
  }
  handle_commandbuffer_t cbuf = _commandbuffers[_current_frame];
  handle_fence_t in_flight_fence = _in_flight_fences[_current_frame];
  handle_semaphore_t image_available_semaphore =
      _image_available_semaphores[_current_frame];
  handle_semaphore_t render_finished_semaphore =
      _render_finished_semaphores[_current_frame];
  _info.context.wait_fence(in_flight_fence);
  auto swapchain_image = _info.context.get_swapchain_next_image_index(
      _swapchain, image_available_semaphore, core::null_handle);
  if (!swapchain_image) {
    _resize = true;
    return;
  }
  check(swapchain_image, "Failed to get next image");
  _next_image = *swapchain_image;
  _info.context.reset_fence(in_flight_fence);
  _info.context.begin_commandbuffer(cbuf);
}

void base_t::end() {
  horizon_profile();
  handle_commandbuffer_t cbuf = _commandbuffers[_current_frame];
  handle_fence_t in_flight_fence = _in_flight_fences[_current_frame];
  handle_semaphore_t image_available_semaphore =
      _image_available_semaphores[_current_frame];
  handle_semaphore_t render_finished_semaphore =
      _render_finished_semaphores[_current_frame];
  _info.context.end_commandbuffer(cbuf);
  _info.context.submit_commandbuffer(
      cbuf, {image_available_semaphore},
      {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
      {render_finished_semaphore}, in_flight_fence);
  if (!_info.context.present_swapchain(_swapchain, _next_image,
                                       {render_finished_semaphore})) {
    _resize = true;
    return;
  }
  _current_frame = (_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void base_t::resize_swapchain() {
  horizon_profile();
  auto [width, height] = _info.window.dimensions();
  _info.context.wait_idle();
  _info.context.destroy_swapchain(_swapchain);
  _swapchain = _info.context.create_swapchain(_info.window);
}

void base_t::begin_swapchain_renderpass() {
  horizon_profile();
  handle_commandbuffer_t cbuf = _commandbuffers[_current_frame];
  _info.context.cmd_image_memory_barrier(
      cbuf, _info.context.get_swapchain_images(_swapchain)[_next_image],
      VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
  auto rendering_attachment = swapchain_rendering_attachment(
      {0, 0, 0, 0}, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  auto [width, height] = _info.window.dimensions();
  _info.context.cmd_begin_rendering(
      cbuf, {rendering_attachment}, std::nullopt,
      VkRect2D{VkOffset2D{},
               {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}});
}

void base_t::end_swapchain_renderpass() {
  horizon_profile();
  handle_commandbuffer_t cbuf = _commandbuffers[_current_frame];
  _info.context.cmd_end_rendering(cbuf);
  _info.context.cmd_image_memory_barrier(
      cbuf, _info.context.get_swapchain_images(_swapchain)[_next_image],
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
}

handle_managed_buffer_t
base_t::create_buffer(resource_update_policy_t update_policy,
                      const config_buffer_t &config) {
  horizon_profile();
  internal::managed_buffer_t<MAX_FRAMES_IN_FLIGHT> managed_buffer{
      .update_policy = update_policy};
  if (update_policy == resource_update_policy_t::e_sparse) {
    managed_buffer.handle_buffers[0] = _info.context.create_buffer(config);
  } else if (update_policy == resource_update_policy_t::e_every_frame) {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
      managed_buffer.handle_buffers[i] = _info.context.create_buffer(config);
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

handle_managed_descriptor_set_t
base_t::allocate_descriptor_set(resource_update_policy_t update_policy,
                                const config_descriptor_set_t &config) {
  horizon_profile();
  internal::managed_descriptor_set_t<MAX_FRAMES_IN_FLIGHT>
      managed_descriptor_set{.update_policy = update_policy};
  if (update_policy == resource_update_policy_t::e_sparse) {
    managed_descriptor_set.handle_descriptor_sets[0] =
        _info.context.allocate_descriptor_set(config);
  } else if (managed_descriptor_set.update_policy ==
             resource_update_policy_t::e_every_frame) {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      managed_descriptor_set.handle_descriptor_sets[i] =
          _info.context.allocate_descriptor_set(config);
    }
  }
  handle_managed_descriptor_set_t handle =
      utils::create_and_insert_new_handle<handle_managed_descriptor_set_t>(
          _descriptor_sets, managed_descriptor_set);
  return handle;
}

handle_descriptor_set_t
base_t::descriptor_set(handle_managed_descriptor_set_t handle) {
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
  return _info.context.get_swapchain_images(_swapchain)[_next_image];
}

handle_image_view_t base_t::current_swapchain_image_view() {
  horizon_profile();
  return _info.context.get_swapchain_image_views(_swapchain)[_next_image];
}

rendering_attachment_t base_t::swapchain_rendering_attachment(
    VkClearValue vk_clear_value, VkImageLayout vk_layout,
    VkAttachmentLoadOp vk_load_op, VkAttachmentStoreOp vk_store_op) {
  horizon_profile();
  rendering_attachment_t rendering_attachment{};
  rendering_attachment.clear_value = vk_clear_value;
  rendering_attachment.image_layout = vk_layout;
  rendering_attachment.load_op = vk_load_op;
  rendering_attachment.store_op = vk_store_op;
  rendering_attachment.handle_image_view =
      _info.context.get_swapchain_image_views(_swapchain)[_next_image];
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

void base_t::set_bindless_image(handle_bindless_image_t handle,
                                handle_image_view_t image_view,
                                VkImageLayout vk_image_layout) {
  horizon_profile();
  _info.context.update_descriptor_set(_bindless_descriptor_set)
      .push_image_write(
          0,
          {.handle_image_view = image_view, .vk_image_layout = vk_image_layout},
          static_cast<uint32_t>(handle))
      .commit();
}
void base_t::set_bindless_sampler(handle_bindless_sampler_t handle,
                                  handle_sampler_t sampler) {
  horizon_profile();
  _info.context.update_descriptor_set(_bindless_descriptor_set)
      .push_image_write(1, {.handle_sampler = sampler},
                        static_cast<uint32_t>(handle))
      .commit();
}

} // namespace gfx

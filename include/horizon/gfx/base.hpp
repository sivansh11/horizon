#ifndef GFX_BASE_HPP
#define GFX_BASE_HPP

#include "horizon/core/core.hpp"
#include "horizon/core/window.hpp"
#include "horizon/gfx/context.hpp"
#include "horizon/gfx/types.hpp"
#include <vulkan/vulkan_core.h>

namespace gfx {

define_handle(handle_managed_buffer_t);
define_handle(handle_managed_descriptor_set_t);

define_handle(handle_bindless_image_t);
define_handle(handle_bindless_sampler_t);

enum class resource_update_policy_t {
  e_sparse,
  e_every_frame,
  // e_ping_pong
};

namespace internal {

template <size_t MAX_FRAMES_IN_FLIGHT> struct managed_buffer_t {
  std::array<handle_buffer_t, MAX_FRAMES_IN_FLIGHT> handle_buffers;
  resource_update_policy_t update_policy;
};

template <size_t MAX_FRAMES_IN_FLIGHT> struct managed_descriptor_set_t {
  std::array<handle_descriptor_set_t, MAX_FRAMES_IN_FLIGHT>
      handle_descriptor_sets;
  resource_update_policy_t update_policy;
};

} // namespace internal

struct managed_buffer_descriptor_info_t {
  handle_managed_buffer_t handle = core::null_handle;
  VkDeviceSize vk_offset = 0;
  VkDeviceSize vk_range = VK_WHOLE_SIZE;
};

enum class managed_descriptor_info_type_t {
  e_buffer,
  e_image,
};

struct managed_descriptor_info_t {
  uint32_t binding;
  uint32_t array_element;
  managed_descriptor_info_type_t type;
  union as_t {
    managed_buffer_descriptor_info_t buffer_info;
    image_descriptor_info_t image_info;
  } as;
};

struct base_t;

template <size_t MAX_FRAMES_IN_FLIGHT> struct update_managed_descriptor_set_t;

struct base_t;

struct base_t {
  constexpr static size_t MAX_FRAMES_IN_FLIGHT = 2;

  base_t(core::ref<core::window_t> window, core::ref<context_t> context);
  ~base_t();

  void begin();
  void end();

  void resize_swapchain();

  void begin_swapchain_renderpass();
  void end_swapchain_renderpass();

  handle_managed_buffer_t create_buffer(resource_update_policy_t update_policy,
                                        const config_buffer_t &config);
  handle_buffer_t buffer(handle_managed_buffer_t handle);

  handle_managed_descriptor_set_t
  allocate_descriptor_set(resource_update_policy_t update_policy,
                          const config_descriptor_set_t &config);
  handle_descriptor_set_t
  descriptor_set(handle_managed_descriptor_set_t handle);
  update_managed_descriptor_set_t<MAX_FRAMES_IN_FLIGHT>
  update_managed_descriptor_set(handle_managed_descriptor_set_t handle);

  rendering_attachment_t swapchain_rendering_attachment(
      VkClearValue vk_clear_value, VkImageLayout vk_layout,
      VkAttachmentLoadOp vk_load_op, VkAttachmentStoreOp vk_store_op);

  handle_commandbuffer_t current_commandbuffer();
  uint32_t current_frame();
  handle_image_t current_swapchain_image();
  handle_image_view_t current_swapchain_image_view();

  handle_bindless_image_t new_bindless_image();
  handle_bindless_sampler_t new_bindless_sampler();

  void set_bindless_image(handle_bindless_image_t handle,
                          handle_image_view_t image_view,
                          VkImageLayout vk_image_layout);
  void set_bindless_sampler(handle_bindless_sampler_t handle,
                            handle_sampler_t sampler);

  core::ref<core::window_t> _window;
  core::ref<context_t> _context;
  handle_swapchain_t _swapchain;
  handle_command_pool_t _command_pool;
  handle_commandbuffer_t _commandbuffers[MAX_FRAMES_IN_FLIGHT];
  handle_fence_t _in_flight_fences[MAX_FRAMES_IN_FLIGHT];
  handle_semaphore_t _image_available_semaphores[MAX_FRAMES_IN_FLIGHT];
  handle_semaphore_t _render_finished_semaphores[MAX_FRAMES_IN_FLIGHT];

  handle_descriptor_set_layout_t _bindless_descriptor_set_layout;
  handle_descriptor_set_t _bindless_descriptor_set;

  uint32_t _current_frame = 0;
  uint32_t _next_image = 0;

  handle_bindless_image_t _image_counter = 0;
  handle_bindless_sampler_t _sampler_counter = 0;

  bool _resize = false;

  std::map<handle_managed_buffer_t,
           internal::managed_buffer_t<MAX_FRAMES_IN_FLIGHT>>
      _buffers;
  std::map<handle_managed_descriptor_set_t,
           internal::managed_descriptor_set_t<MAX_FRAMES_IN_FLIGHT>>
      _descriptor_sets;

  handle_pipeline_t _swapchain_pipeline;
  handle_descriptor_set_t _swapchain_descriptor_set;
};

template <size_t MAX_FRAMES_IN_FLIGHT> struct update_managed_descriptor_set_t {
  update_managed_descriptor_set_t &
  push_buffer_write(uint32_t binding,
                    const managed_buffer_descriptor_info_t &info,
                    uint32_t array_element) {
    horizon_profile();
    internal::managed_descriptor_set_t<MAX_FRAMES_IN_FLIGHT>
        &managed_descriptor_set = utils::assert_and_get_data<
            internal::managed_descriptor_set_t<MAX_FRAMES_IN_FLIGHT>>(
            handle, base._descriptor_sets);
    internal::managed_buffer_t<MAX_FRAMES_IN_FLIGHT> &managed_buffer =
        utils::assert_and_get_data<internal::buffer_t>(handle, base._buffers);
    if (managed_descriptor_set.update_policy ==
        resource_update_policy_t::e_sparse) {
      check(managed_buffer.update_policy == resource_update_policy_t::e_sparse,
            "A sparse descriptor must only point to a sparse buffer!");
    }

    managed_descriptor_info_t managed_descriptor_info{};
    managed_descriptor_info.binding = binding;
    managed_descriptor_info.array_element = array_element;
    managed_descriptor_info.type = managed_descriptor_info_type_t::e_buffer;
    managed_descriptor_info.as.buffer_info = info;
    infos.push_back(managed_descriptor_info);
    return *this;
  }

  update_managed_descriptor_set_t &
  push_image_write(uint32_t binding, const image_descriptor_info_t &info,
                   uint32_t array_element = 0) {
    horizon_profile();
    managed_descriptor_info_t managed_descriptor_info{};
    managed_descriptor_info.binding = binding;
    managed_descriptor_info.array_element = array_element;
    managed_descriptor_info.type = managed_descriptor_info_type_t::e_image;
    managed_descriptor_info.as.image_info = info;
    infos.push_back(managed_descriptor_info);
    return *this;
  }

  void commit() {
    horizon_profile();
    internal::managed_descriptor_set_t<MAX_FRAMES_IN_FLIGHT>
        managed_descriptor_set = utils::assert_and_get_data<
            internal::managed_descriptor_set_t<MAX_FRAMES_IN_FLIGHT>>(
            handle, base._descriptor_sets);
    if (managed_descriptor_set.update_policy ==
        resource_update_policy_t::e_sparse) {
      auto update_descriptor_set = base._context->update_descriptor_set(
          managed_descriptor_set.handle_descriptor_sets[0]);
      for (auto &info : infos) {
        if (info.type == managed_descriptor_info_type_t::e_buffer) {
          internal::managed_buffer_t<MAX_FRAMES_IN_FLIGHT> &buffer =
              utils::assert_and_get_data<
                  internal::managed_buffer_t<MAX_FRAMES_IN_FLIGHT>>(
                  info.as.buffer_info.handle, base._buffers);
          buffer_descriptor_info_t gfx_info{};
          gfx_info.handle_buffer = buffer.handle_buffers[0];
          gfx_info.vk_offset = info.as.buffer_info.vk_offset;
          gfx_info.vk_range = info.as.buffer_info.vk_range;
          update_descriptor_set.push_buffer_write(info.binding, gfx_info,
                                                  info.array_element);
        } else if (info.type == managed_descriptor_info_type_t::e_image) {
          image_descriptor_info_t gfx_info{};
          gfx_info = info.as.image_info;
          update_descriptor_set.push_buffer_write(info.binding, gfx_info,
                                                  info.array_element);
        }
      }
    } else if (managed_descriptor_set.update_policy ==
               resource_update_policy_t::e_every_frame) {
      // single image
      // single/multiple buffer
      for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        auto update_descriptor_set = base._context->update_descriptor_set(
            managed_descriptor_set.handle_descriptor_sets[i]);
        for (auto &info : infos) {
          if (info.type == managed_descriptor_info_type_t::e_buffer) {
            internal::managed_buffer_t<MAX_FRAMES_IN_FLIGHT> &managed_buffer =
                utils::assert_and_get_data<
                    internal::managed_buffer_t<MAX_FRAMES_IN_FLIGHT>>(
                    info.as.buffer_info.handle, base._buffers);
            if (managed_buffer.update_policy ==
                resource_update_policy_t::e_sparse) {
              buffer_descriptor_info_t gfx_info{};
              gfx_info.handle_buffer = managed_buffer.handle_buffers[0];
              gfx_info.vk_offset = info.as.buffer_info.vk_offset;
              gfx_info.vk_range = info.as.buffer_info.vk_range;
              update_descriptor_set.push_buffer_write(info.binding, gfx_info,
                                                      info.array_element);
            } else if (managed_buffer.update_policy ==
                       resource_update_policy_t::e_every_frame) {
              buffer_descriptor_info_t gfx_info{};
              gfx_info.handle_buffer = managed_buffer.handle_buffers[i];
              gfx_info.vk_offset = info.as.buffer_info.vk_offset;
              gfx_info.vk_range = info.as.buffer_info.vk_range;
              update_descriptor_set.push_buffer_write(info.binding, gfx_info,
                                                      info.array_element);
            }
          } else if (info.type == managed_descriptor_info_type_t::e_image) {
            image_descriptor_info_t gfx_info{};
            gfx_info = info.as.image_info;
            update_descriptor_set.push_image_write(info.binding, gfx_info,
                                                   info.array_element);
          }
        }
        update_descriptor_set.commit();
      }
    }
  }

  base_t &base;
  handle_managed_descriptor_set_t handle = core::null_handle;
  std::vector<managed_descriptor_info_t> infos;
};

} // namespace gfx

#endif

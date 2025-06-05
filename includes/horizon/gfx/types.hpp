#ifndef GFX_TYPES_HPP
#define GFX_TYPES_HPP

#include "horizon/core/core.hpp"

namespace gfx {

define_handle(handle_swapchain_t);
define_handle(handle_buffer_t);
define_handle(handle_sampler_t);
define_handle(handle_image_t);
define_handle(handle_image_view_t);
define_handle(handle_descriptor_set_layout_t);
define_handle(handle_descriptor_set_t);
define_handle(handle_shader_t);
define_handle(handle_pipeline_layout_t);
define_handle(handle_pipeline_t);
define_handle(handle_fence_t);
define_handle(handle_semaphore_t);
define_handle(handle_command_pool_t);
define_handle(handle_commandbuffer_t);
define_handle(handle_timer_t);

} // namespace gfx

#endif

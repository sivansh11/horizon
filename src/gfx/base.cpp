#include "horizon/gfx/base.hpp"
#include "horizon/core/core.hpp"
#include "horizon/core/logger.hpp"
#include "horizon/gfx/context.hpp"
#include "horizon/gfx/helper.hpp"

namespace gfx {

base_t::base_t(const base_config_t& info) : _info(info) {
    horizon_profile();
    _swapchain          = _info.context.create_swapchain(_info.window);
    _command_pool       = _info.context.create_command_pool({});
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        _commandbuffers[i] = _info.context.allocate_commandbuffer(
            { .handle_command_pool = _command_pool, .debug_name = "commandbuffer_" + std::to_string(i) }
        );
        _in_flight_fences[i] = _info.context.create_fence({});
        _image_available_semaphores[i] = _info.context.create_semaphore({});
        _render_finished_semaphores[i] = _info.context.create_semaphore({});
    }
    config_descriptor_set_layout_t config_swapchain_descriptor_set_layout{};
    config_swapchain_descriptor_set_layout.add_layout_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    handle_descriptor_set_layout_t swapchain_descriptor_set_layout = _info.context.create_descriptor_set_layout(config_swapchain_descriptor_set_layout);

    config_pipeline_layout_t config_swapchain_pipeline_layout{};
    config_swapchain_pipeline_layout.add_descriptor_set_layout(swapchain_descriptor_set_layout);
    handle_pipeline_layout_t swapchain_pipeline_layout = _info.context.create_pipeline_layout(config_swapchain_pipeline_layout);

    const char *vertex_shader_code = R"(
        #version 450
        layout (location = 0) out vec2 out_uv;
        vec2 positions[6] = vec2[](
            vec2(-1, -1),
            vec2(-1,  1),
            vec2( 1,  1),
            vec2(-1, -1),
            vec2( 1,  1),
            vec2( 1, -1)
        );
        vec2 uv[6] = vec2[](
            //vec2(0, 1),
            //vec2(0, 0),
            //vec2(1, 0),
            //vec2(0, 1),
            //vec2(1, 0),
            //vec2(1, 1)
            vec2(0, 0),
            vec2(0, 1),
            vec2(1, 1),
            vec2(0, 0),
            vec2(1, 1),
            vec2(1, 0)
        );
        void main() {
            gl_Position = vec4(positions[gl_VertexIndex], 0, 1);
            out_uv = uv[gl_VertexIndex];
        }
    )";

    const char *fragment_shader_code = R"(
        #version 450
        layout (location = 0) in vec2 uv;
        layout (location = 0) out vec4 out_color;
        layout (set = 0, binding = 0) uniform sampler2D screen;
        void main() {
            out_color = texture(screen, uv);
        }
    )";

    handle_shader_t swapchain_vertex_shader = _info.context.create_shader(config_shader_t{ .code_or_path = vertex_shader_code, .is_code = true, .name = "swapchain vertex shader", .type = shader_type_t::e_vertex, .language = shader_language_t::e_glsl });
    handle_shader_t swapchain_fragment_shader = _info.context.create_shader(config_shader_t{ .code_or_path = fragment_shader_code, .is_code = true, .name = "swapchain fragment shader", .type = shader_type_t::e_fragment, .language = shader_language_t::e_glsl });

    config_pipeline_t config_swapchain_pipeline{};
    config_swapchain_pipeline.handle_pipeline_layout = swapchain_pipeline_layout;
    config_swapchain_pipeline.add_color_attachment(VK_FORMAT_B8G8R8A8_SRGB, default_color_blend_attachment())
                             .add_shader(swapchain_vertex_shader)
                             .add_shader(swapchain_fragment_shader);
    _swapchain_pipeline = _info.context.create_graphics_pipeline(config_swapchain_pipeline);

    _swapchain_descriptor_set = _info.context.allocate_descriptor_set({ .handle_descriptor_set_layout = swapchain_descriptor_set_layout });
    _info.context.update_descriptor_set(_swapchain_descriptor_set)
        .push_image_write(0, image_descriptor_info_t{ .handle_sampler = _info.sampler, .handle_image_view = _info.final_image_view, .vk_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL })
        .commit();
}

base_t::~base_t() {
    horizon_profile();
    _info.context.wait_idle();
    for (auto& [handle, buffer] : _buffers) {
        for (auto handle_buffer : buffer.handle_buffers) {
            _info.context.destroy_buffer(handle_buffer);
        }
    }
    for (auto& [handle, descriptor_set] : _descriptor_sets) {
        for (auto handle_descriptor_set : descriptor_set.handle_descriptor_sets) {
            _info.context.free_descriptor_set(handle_descriptor_set);
        }
    }
}

void base_t::begin(bool transition_swapchain_image) {
    horizon_profile();
    handle_commandbuffer_t cbuf = _commandbuffers[_current_frame];
    handle_fence_t in_flight_fence = _in_flight_fences[_current_frame];
    handle_semaphore_t image_available_semaphore = _image_available_semaphores[_current_frame];
    handle_semaphore_t render_finished_semaphore = _render_finished_semaphores[_current_frame];
    _info.context.wait_fence(in_flight_fence);
    auto swapchain_image = _info.context.get_swapchain_next_image_index(_swapchain, image_available_semaphore, core::null_handle);
    check(swapchain_image, "Failed to get next image");
    _next_image = *swapchain_image;
    _info.context.reset_fence(in_flight_fence);
    _info.context.begin_commandbuffer(cbuf);
    if (transition_swapchain_image) 
        _info.context.cmd_image_memory_barrier(cbuf,
                                               _info.context.get_swapchain_images(_swapchain)[_next_image],
                                               VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                               0,
                                               VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
}

void base_t::end() {
    horizon_profile();
    handle_commandbuffer_t cbuf = _commandbuffers[_current_frame];
    auto rendering_attachment = swapchain_rendering_attachment({ 0, 0, 0, 0 }, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
    auto [width, height] = _info.window.dimensions();
    _info.context.cmd_begin_rendering(cbuf, { rendering_attachment }, std::nullopt, VkRect2D{ VkOffset2D{}, { static_cast<uint32_t>(width), static_cast<uint32_t>(height) } });
    _info.context.cmd_bind_pipeline(cbuf, _swapchain_pipeline);
    _info.context.cmd_bind_descriptor_sets(cbuf, _swapchain_pipeline, 0, { _swapchain_descriptor_set });
    auto [viewport, scissor] = helper::fill_viewport_and_scissor_structs(width, height);
    _info.context.cmd_set_viewport_and_scissor(cbuf, viewport, scissor);
    _info.context.cmd_draw(cbuf, 6, 1, 0, 0);
    _info.context.cmd_end_rendering(cbuf);
    handle_fence_t in_flight_fence = _in_flight_fences[_current_frame];
    handle_semaphore_t image_available_semaphore = _image_available_semaphores[_current_frame];
    handle_semaphore_t render_finished_semaphore = _render_finished_semaphores[_current_frame];
    _info.context.cmd_image_memory_barrier(cbuf,
                                           _info.context.get_swapchain_images(_swapchain)[_next_image],
                                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                           VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                           VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                           0,
                                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                           VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    _info.context.end_commandbuffer(cbuf);
    _info.context.submit_commandbuffer(cbuf, { image_available_semaphore }, { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }, { render_finished_semaphore }, in_flight_fence);
    _info.context.present_swapchain(_swapchain, _next_image, { render_finished_semaphore });
    _current_frame = (_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

handle_managed_buffer_t base_t::create_buffer(resource_update_policy_t update_policy, const config_buffer_t& config) {
    horizon_profile();
    internal::managed_buffer_t<MAX_FRAMES_IN_FLIGHT> managed_buffer{ .update_policy = update_policy };
    if (update_policy == resource_update_policy_t::e_sparse) {
        managed_buffer.handle_buffers[0] = _info.context.create_buffer(config);
    } else if (update_policy == resource_update_policy_t::e_every_frame) {
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            managed_buffer.handle_buffers[i] = _info.context.create_buffer(config);
    }
    handle_managed_buffer_t handle = utils::create_and_insert_new_handle<handle_managed_buffer_t>(_buffers, managed_buffer);
    horizon_trace("created managed buffer");
    return handle;
}

handle_buffer_t base_t::buffer(handle_managed_buffer_t handle) {
    horizon_profile();
    internal::managed_buffer_t<MAX_FRAMES_IN_FLIGHT>& managed_buffer = utils::assert_and_get_data<internal::managed_buffer_t<MAX_FRAMES_IN_FLIGHT>>(handle, _buffers);
    if (managed_buffer.update_policy == resource_update_policy_t::e_sparse) {
        return managed_buffer.handle_buffers[0];
    } else if (managed_buffer.update_policy == resource_update_policy_t::e_every_frame) {
        return managed_buffer.handle_buffers[_current_frame];
    }
    check(false, "reached unreachable");
}

handle_managed_descriptor_set_t base_t::allocate_descriptor_set(resource_update_policy_t update_policy, const config_descriptor_set_t& config) {
    horizon_profile();
    internal::managed_descriptor_set_t<MAX_FRAMES_IN_FLIGHT> managed_descriptor_set{ .update_policy = update_policy };
    if (update_policy == resource_update_policy_t::e_sparse) {
        managed_descriptor_set.handle_descriptor_sets[0] = _info.context.allocate_descriptor_set(config);
    } else if (managed_descriptor_set.update_policy == resource_update_policy_t::e_every_frame) {
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            managed_descriptor_set.handle_descriptor_sets[i] = _info.context.allocate_descriptor_set(config);
        }
    }
    handle_managed_descriptor_set_t handle = utils::create_and_insert_new_handle<handle_managed_descriptor_set_t>(_descriptor_sets, managed_descriptor_set);
    return handle;
}

handle_descriptor_set_t base_t::descriptor_set(handle_managed_descriptor_set_t handle) {
    horizon_profile();
    internal::managed_descriptor_set_t<MAX_FRAMES_IN_FLIGHT>& managed_descriptor_set = utils::assert_and_get_data<internal::managed_descriptor_set_t<MAX_FRAMES_IN_FLIGHT>>(handle, _descriptor_sets);
    if (managed_descriptor_set.update_policy == resource_update_policy_t::e_sparse) {
        return managed_descriptor_set.handle_descriptor_sets[0];
    } else if (managed_descriptor_set.update_policy == resource_update_policy_t::e_every_frame) {
        return managed_descriptor_set.handle_descriptor_sets[_current_frame];
    }
    check(false, "reached unreachable");
}

update_managed_descriptor_set_t<base_t::MAX_FRAMES_IN_FLIGHT> base_t::update_managed_descriptor_set(handle_managed_descriptor_set_t handle) {
    horizon_profile();
    return { *this, handle };
}

handle_commandbuffer_t base_t::current_commandbuffer() {
    horizon_profile();
    return _commandbuffers[_current_frame];
}

handle_image_t base_t::current_swapchain_image() {
    horizon_profile();
    return _info.context.get_swapchain_images(_swapchain)[_next_image];
}

handle_image_view_t base_t::current_swapchain_image_view() {
    horizon_profile();
    return _info.context.get_swapchain_image_views(_swapchain)[_next_image];
}

rendering_attachment_t base_t::swapchain_rendering_attachment(VkClearValue vk_clear_value, VkImageLayout vk_layout, VkAttachmentLoadOp vk_load_op, VkAttachmentStoreOp vk_store_op) {
    horizon_profile();
    rendering_attachment_t rendering_attachment{};
    rendering_attachment.clear_value = vk_clear_value;
    rendering_attachment.image_layout = vk_layout;
    rendering_attachment.load_op = vk_load_op;
    rendering_attachment.store_op = vk_store_op;
    rendering_attachment.handle_image_view = _info.context.get_swapchain_image_views(_swapchain)[_next_image];
    return rendering_attachment;
}

}

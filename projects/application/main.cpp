#include "core/core.hpp"
#include "core/window.hpp"

#include "gfx/context.hpp"
#include "gfx/base_renderer.hpp"
#include "gfx/helper.hpp"
#include "gfx/bindless_manager.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

int main(int argc, char **argv) {
    
    core::log_t::set_log_level(core::log_level_t::e_info);

    auto window = core::window_t{ "non bindless", 1200, 800 };
    auto context = gfx::context_t{ true };

    VkFormat o_image_format = VK_FORMAT_R8G8B8A8_SRGB;

    auto [width, height] = window.dimensions();
    auto [o_image, o_image_view] = gfx::helper::create_2D_image_and_image_view(context, width, height, o_image_format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    auto sampler = context.create_sampler({});

    auto renderer = renderer::base_renderer_t{ window, context, sampler, o_image_view };

    gfx::helper::imgui_init(window, renderer, o_image_format);

    while (!window.should_close()) {
        core::clear_frame_function_times();
        core::window_t::poll_events();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE)) break;

        renderer.begin();
        auto cbuff = renderer.current_commandbuffer();
        auto [viewport, scissor] = gfx::helper::fill_viewport_and_scissor_structs(width, height);

        context.cmd_image_memory_barrier(cbuff, o_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        gfx::rendering_attachment_t color_attachment{};
        color_attachment.clear_value = {0, 0, 0, 0};
        color_attachment.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.handle_image_view = o_image_view;
        context.cmd_begin_rendering(cbuff, { color_attachment }, std::nullopt, VkRect2D{ VkOffset2D{}, { static_cast<uint32_t>(width), static_cast<uint32_t>(height) } });
        gfx::helper::imgui_newframe();

        ImGui::Begin("Test");
        ImGui::End();

        gfx::helper::imgui_endframe(renderer, cbuff);
        context.cmd_end_rendering(cbuff);

        context.cmd_image_memory_barrier(cbuff, o_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

        renderer.end();
    }

    context.wait_idle();

    gfx::helper::imgui_shutdown();

    return 0;
}

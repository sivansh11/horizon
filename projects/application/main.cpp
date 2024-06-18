#include "core/core.hpp"
#include "core/window.hpp"

#include "gfx/context.hpp"
#include "gfx/base_renderer.hpp"
#include "gfx/helper.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

int main(int argc, char **argv) {
    
    core::log_t::set_log_level(core::log_level_t::e_info);

    core::window_t window{ "application", 1200, 800 };
    auto [width, height] = window.dimensions();

    gfx::context_t context{ true };

    VkFormat final_image_format = VK_FORMAT_R8G8B8A8_SRGB;
    gfx::config_image_t config_target_image{};
    config_target_image.vk_width = width;
    config_target_image.vk_height = height;
    config_target_image.vk_depth = 1;
    config_target_image.vk_type = VK_IMAGE_TYPE_2D;
    config_target_image.vk_format = final_image_format;
    config_target_image.vk_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    config_target_image.vk_mips = 1;
    gfx::handle_image_t final_image = context.create_image(config_target_image);
    gfx::handle_image_view_t final_image_view = context.create_image_view({ .handle_image = final_image });
    gfx::handle_sampler_t default_sampler = context.create_sampler({});
    renderer::base_renderer_t renderer{ window, context, default_sampler, final_image_view };

    gfx::helper::imgui_init(window, renderer, final_image_format);

    while (!window.should_close()) {
        core::clear_frame_function_times();
        core::window_t::poll_events();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE)) break;

        renderer.begin();
        auto cbuff = renderer.current_commandbuffer();
        auto [viewport, scissor] = gfx::helper::fill_viewport_and_scissor_structs(width, height);

        context.cmd_image_memory_barrier(cbuff, final_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        gfx::rendering_attachment_t color_attachment{};
        color_attachment.clear_value = {0, 0, 0, 0};
        color_attachment.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.handle_image_view = final_image_view;
        context.cmd_begin_rendering(cbuff, { color_attachment }, std::nullopt, VkRect2D{ VkOffset2D{}, { static_cast<uint32_t>(width), static_cast<uint32_t>(height) } });
        gfx::helper::imgui_newframe();

        ImGui::Begin("Test");
        ImGui::End();

        gfx::helper::imgui_endframe(renderer, cbuff);
        context.cmd_end_rendering(cbuff);

        context.cmd_image_memory_barrier(cbuff, final_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

        renderer.end();
    }

    context.wait_idle();

    gfx::helper::imgui_shutdown();

    return 0;
}

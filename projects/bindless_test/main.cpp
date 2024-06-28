#include "core/window.hpp"

#include "gfx/context.hpp"
#include "gfx/helper.hpp"
#include "gfx/base_renderer.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>


int main(int argc, char **argv) {
    auto context = gfx::context_t{ true };
    auto window = core::window_t{ "bindless test", 800, 800 };

    auto [width, height] = window.dimensions();
    auto [target_image, target_image_view] = gfx::helper::create_2D_image_and_image_views(context, width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    auto sampler = context.create_sampler({});

    auto renderer = renderer::base_renderer_t{ window, context, sampler, target_image_view };

    auto config_pipeline_layout = gfx::config_pipeline_layout_t{};
    auto pipeline_layout = context.create_pipeline_layout(config_pipeline_layout);

    auto config_pipeline = gfx::config_pipeline_t{};
    config_pipeline.handle_pipeline_layout = pipeline_layout;
    config_pipeline.add_shader(context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/bindless_test/vert.slang", .is_code = false, .name = "vertex shader", .type = gfx::shader_type_t::e_vertex }));
    config_pipeline.add_shader(context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/bindless_test/frag.slang", .is_code = false, .name = "fragment shader", .type = gfx::shader_type_t::e_fragment }));
    config_pipeline.add_color_attachment(VK_FORMAT_R8G8B8A8_UNORM, gfx::default_color_blend_attachment());
    auto pipeline = context.create_graphics_pipeline(config_pipeline);

    while (!window.should_close()) {
        core::window_t::poll_events();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE)) break;

        renderer.begin();
        auto commandbuffer = renderer.current_commandbuffer();
        auto [viewport, scissor] = gfx::helper::fill_viewport_and_scissor_structs(width, height);

        gfx::rendering_attachment_t color_rendering_attachment{};
        color_rendering_attachment.clear_value = { 0, 0, 0, 0 };
        color_rendering_attachment.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_rendering_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_rendering_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
        color_rendering_attachment.handle_image_view = target_image_view;

        context.cmd_image_memory_barrier(commandbuffer, target_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        context.cmd_begin_rendering(commandbuffer, { color_rendering_attachment }, std::nullopt, VkRect2D{ VkOffset2D{}, { static_cast<uint32_t>(width), static_cast<uint32_t>(height) } });
        context.cmd_bind_pipeline(commandbuffer, pipeline);
        context.cmd_set_viewport_and_scissor(commandbuffer, viewport, scissor);
        context.cmd_draw(commandbuffer, 6, 1, 0, 0);

        context.cmd_image_memory_barrier(commandbuffer, target_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

        renderer.end();
    }

    context.wait_idle();

    return 0;
}

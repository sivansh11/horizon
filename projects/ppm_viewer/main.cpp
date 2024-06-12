#include "test.hpp"

#define horizon_profile_enable
#include "core/core.hpp"
#include "core/window.hpp"

#include "gfx/context.hpp"
#include "gfx/base_renderer.hpp"

#include "filewatcher.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <thread>

int main(int argc, char ** argv) {
    
    if (argc != 2) {
        horizon_error("please give the ppm file path");
        exit(EXIT_FAILURE);
    }

    core::log_t::set_log_level(core::log_level_t::e_info);

    core::window_t window{ "live ppm viewer", 640, 640 };
    gfx::context_t context{ true };

    auto [width, height] = window.dimensions();

    gfx::config_image_t config_target_image{};
    config_target_image.vk_width = width;
    config_target_image.vk_height = height;
    config_target_image.vk_depth = 1;
    config_target_image.vk_type = VK_IMAGE_TYPE_2D;
    config_target_image.vk_format = VK_FORMAT_R8G8B8A8_SRGB;
    config_target_image.vk_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    config_target_image.vk_mips = 1;
    gfx::handle_image_t target_image = context.create_image(config_target_image);
    gfx::handle_image_view_t target_image_view = context.create_image_view({ .handle_image = target_image });

    gfx::handle_sampler_t sampler = context.create_sampler({});

    renderer::base_renderer_t renderer{ window, context, sampler, target_image_view };

    gfx::config_descriptor_set_layout_t config_descriptor_set_layout{};
    config_descriptor_set_layout.add_layout_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    gfx::handle_descriptor_set_layout_t descriptor_set_layout = context.create_descriptor_set_layout(config_descriptor_set_layout);

    gfx::config_pipeline_layout_t config_pipeline_layout{};
    config_pipeline_layout.add_descriptor_set_layout(descriptor_set_layout);
    gfx::handle_pipeline_layout_t pipeline_layout = context.create_pipeline_layout(config_pipeline_layout);

    gfx::handle_shader_t vertex_shader = context.create_shader(gfx::config_shader_t{.code = core::read_file("../../assets/shaders/ppm_viewer/glsl.vert").data(), .name = "ppm_viewer vertex shader", .type = gfx::shader_type_t::e_vertex });
    gfx::handle_shader_t fragment_shader = context.create_shader(gfx::config_shader_t{.code = core::read_file("../../assets/shaders/ppm_viewer/glsl.frag").data(), .name = "ppm_viewer fragment shader", .type = gfx::shader_type_t::e_fragment });

    gfx::config_pipeline_t config_pipeline{};
    config_pipeline.handle_pipeline_layout = pipeline_layout;
    config_pipeline.add_color_attachment(VK_FORMAT_B8G8R8A8_SRGB, gfx::default_color_blend_attachment())
                             .add_shader(vertex_shader)
                             .add_shader(fragment_shader);
    gfx::handle_pipeline_t pipeline = context.create_graphics_pipeline(config_pipeline);

    filewatcher_t ppm_file_watcher{ argv[1] };

    gfx::handle_image_t ppm_image = gfx::helper::load_image_from_path(context, renderer.command_pool, argv[1], VK_FORMAT_R8G8B8A8_SRGB);
    gfx::handle_image_view_t ppm_image_view = context.create_image_view({ .handle_image = ppm_image });

    gfx::handle_descriptor_set_t descriptor_set = context.allocate_descriptor_set({ .handle_descriptor_set_layout = descriptor_set_layout });
    context.update_descriptor_set(descriptor_set)
        .push_image_write(0, { .handle_sampler = sampler, .handle_image_view = ppm_image_view, .vk_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL })
        .commit();

    core::frame_timer_t frame_timer{ 30.f };
    while (!window.should_close()) {
        core::clear_frame_function_times();
        core::window_t::poll_events();

        if (glfwGetKey(window.window(), GLFW_KEY_ESCAPE)) break;

        if (ppm_file_watcher.has_changed()) {
            std::this_thread::sleep_for(std::chrono::seconds{ 1 });
            context.wait_idle();
            
            context.destroy_image_view(ppm_image_view);
            context.destroy_image(ppm_image);

            ppm_image = gfx::helper::load_image_from_path(context, renderer.command_pool, argv[1], VK_FORMAT_R8G8B8A8_SRGB);
            ppm_image_view = context.create_image_view({ .handle_image = ppm_image });

            context.update_descriptor_set(descriptor_set)
                .push_image_write(0, { .handle_sampler = sampler, .handle_image_view = ppm_image_view, .vk_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL })
                .commit();
        }

        auto dt = frame_timer.update();

        renderer.begin();
        auto commandbuffer = renderer.current_commandbuffer();
        auto [viewport, scissor] = gfx::helper::fill_viewport_and_scissor_structs(width, height);

        gfx::rendering_attachment_t rendering_attachment{};
        rendering_attachment.clear_value = {0, 0, 0, 0};
        rendering_attachment.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        rendering_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        rendering_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
        rendering_attachment.handle_image_view = target_image_view;

        context.cmd_image_memory_barrier(commandbuffer, target_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        context.cmd_begin_rendering(commandbuffer, { rendering_attachment }, std::nullopt, VkRect2D{ VkOffset2D{}, { static_cast<uint32_t>(width), static_cast<uint32_t>(height) } });
        context.cmd_bind_pipeline(commandbuffer, pipeline);
        context.cmd_set_viewport_and_scissor(commandbuffer, viewport, scissor);
        context.cmd_bind_descriptor_sets(commandbuffer, pipeline, 0, { descriptor_set });
        context.cmd_draw(commandbuffer, 6, 1, 0, 0);
        context.cmd_end_rendering(commandbuffer);
        context.cmd_image_memory_barrier(commandbuffer, target_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

        renderer.end();
    }

    return 0;
}
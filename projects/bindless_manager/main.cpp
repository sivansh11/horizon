#include "core/core.hpp"
#include "core/window.hpp"
#include "core/model.hpp"

#include "gfx/context.hpp"
#include "gfx/helper.hpp"
#include "gfx/bindless_manager.hpp"
#include "gfx/base_renderer.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

const std::vector<core::vertex_t> vertices {
    core::vertex_t{ {-1 / 2.f, -1 / 2.f, 0}, core::vec3{ 1 }, {0, 0} },
    core::vertex_t{ {-1 / 2.f,  1 / 2.f, 0}, core::vec3{ 1 }, {0, 1} },
    core::vertex_t{ { 1 / 2.f,  1 / 2.f, 0}, core::vec3{ 1 }, {1, 1} },
    core::vertex_t{ {-1 / 2.f, -1 / 2.f, 0}, core::vec3{ 0 }, {0, 0} },
    core::vertex_t{ { 1 / 2.f,  1 / 2.f, 0}, core::vec3{ 0 }, {1, 1} },
    core::vertex_t{ { 1 / 2.f, -1 / 2.f, 0}, core::vec3{ 0 }, {1, 0} },
};

const std::vector<uint32_t> indices {
    0, 1, 2,
    3, 4, 5,
};

int main() {
    
    core::window_t window{ "bindless manager test", 640, 640 };
    gfx::context_t context{ true };

    auto [width, height] = window.dimensions();

    auto [final, final_view] = gfx::helper::create_2D_image_and_image_view(context, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

    auto sampler = context.create_sampler({});

    renderer::base_renderer_t renderer{ window, context, sampler, final_view };

    gfx::bindless_manager_t bindless_manager{ context };

    gfx::config_pipeline_t cp{};
    cp.handle_pipeline_layout = bindless_manager.pl();
    cp.add_color_attachment(VK_FORMAT_R8G8B8A8_SRGB, gfx::default_color_blend_attachment());
    cp.add_shader(context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/bindless_manager/vert.slang", .is_code = false, .name = "vertex", .type = gfx::shader_type_t::e_vertex, .language = gfx::shader_language_t::e_slang }));
    cp.add_shader(context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/bindless_manager/frag.slang", .is_code = false, .name = "fragment", .type = gfx::shader_type_t::e_fragment, .language = gfx::shader_language_t::e_slang }));
    gfx::handle_pipeline_t p = context.create_graphics_pipeline(cp);

    gfx::config_buffer_t config_vertex_buffer{};        
    config_vertex_buffer.vk_size = vertices.size() * sizeof(vertices[0]);
    config_vertex_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    config_vertex_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    gfx::handle_buffer_t vertex_buffer = context.create_buffer(config_vertex_buffer);
    gfx::helper::copy_mappable_buffer(context, vertex_buffer, vertices);

    gfx::config_buffer_t config_index_buffer{};        
    config_index_buffer.vk_size = indices.size() * sizeof(indices[0]);
    config_index_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    config_index_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    gfx::handle_buffer_t index_buffer = context.create_buffer(config_index_buffer);
    gfx::helper::copy_mappable_buffer(context, index_buffer, indices);

    gfx::handle_image_t image = gfx::helper::load_image_from_path(renderer.context, renderer.command_pool, "../../assets/texture/noise.jpg", VK_FORMAT_R8G8B8A8_SRGB);
    gfx::handle_image_view_t image_view = context.create_image_view({ .handle_image = image });

    bindless_manager.slot(vertex_buffer, 0);
    bindless_manager.slot(index_buffer, 1);
    bindless_manager.slot(image_view);      // 0
    bindless_manager.slot(sampler);         // 0

    bindless_manager.flush_updates();

    while (!window.should_close()) {
        core::clear_frame_function_times();
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
        color_rendering_attachment.handle_image_view = final_view;

        context.cmd_image_memory_barrier(commandbuffer, final, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        context.cmd_begin_rendering(commandbuffer, { color_rendering_attachment }, std::nullopt, VkRect2D{ VkOffset2D{}, { static_cast<uint32_t>(width), static_cast<uint32_t>(height) } });
        context.cmd_bind_pipeline(commandbuffer, p);
        context.cmd_bind_descriptor_sets(commandbuffer, p, 0, { bindless_manager.ds() });
        context.cmd_set_viewport_and_scissor(commandbuffer, viewport, scissor);
        context.cmd_draw(commandbuffer, 6, 1, 0, 0);
        context.cmd_end_rendering(commandbuffer);

        context.cmd_image_memory_barrier(commandbuffer, final, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

        renderer.end();
    }

    return 0;
}
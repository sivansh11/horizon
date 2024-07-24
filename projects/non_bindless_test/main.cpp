#include "core/core.hpp"
#include "core/window.hpp"
#include "core/model.hpp"

#include "gfx/context.hpp"
#include "gfx/helper.hpp"
#include "gfx/base_renderer.hpp"

#include <GLFW/glfw3.h>

const std::vector<core::vertex_t> vertices {
    core::vertex_t{ {-1 / 2.f, -1 / 2.f, 0}, {}, {0, 0} },
    core::vertex_t{ {-1 / 2.f,  1 / 2.f, 0}, {}, {0, 1} },
    core::vertex_t{ { 1 / 2.f,  1 / 2.f, 0}, {}, {1, 1} },
    core::vertex_t{ {-1 / 2.f, -1 / 2.f, 0}, {}, {0, 0} },
    core::vertex_t{ { 1 / 2.f,  1 / 2.f, 0}, {}, {1, 1} },
    core::vertex_t{ { 1 / 2.f, -1 / 2.f, 0}, {}, {1, 0} },
};

const std::vector<uint32_t> indices {
    0, 1, 2,
    3, 4, 5,
};

int main(int argc, char **argv) {

    auto window = core::window_t{ "non bindless", 640, 640 };
    auto context = gfx::context_t{ true };

    auto [width, height] = window.dimensions();
    auto [o_image, o_image_view] = gfx::helper::create_2D_image_and_image_view(context, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    auto sampler = context.create_sampler({});

    auto renderer = renderer::base_renderer_t{ window, context, sampler, o_image_view };

    gfx::config_descriptor_set_layout_t cdsl{};
    cdsl.add_layout_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
        .add_layout_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
        .add_layout_binding(2, VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_ALL_GRAPHICS)
        .add_layout_binding(3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_ALL_GRAPHICS);
    gfx::handle_descriptor_set_layout_t dsl = context.create_descriptor_set_layout(cdsl);

    gfx::config_pipeline_layout_t cpl{};
    cpl.add_descriptor_set_layout(dsl);
    gfx::handle_pipeline_layout_t pl = context.create_pipeline_layout(cpl);

    gfx::config_pipeline_t cp{};
    cp.handle_pipeline_layout = pl;
    cp.add_color_attachment(VK_FORMAT_R8G8B8A8_SRGB, gfx::default_color_blend_attachment());
    cp.add_shader(context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/non_bindless/vert.slang", .is_code = false, .name = "vertex shader", .type = gfx::shader_type_t::e_vertex }));
    cp.add_shader(context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/non_bindless/frag.slang", .is_code = false, .name = "fragment shader", .type = gfx::shader_type_t::e_fragment }));
    gfx::handle_pipeline_t p = context.create_graphics_pipeline(cp);

    gfx::config_buffer_t config_index_buffer{};        
    config_index_buffer.vk_size = indices.size() * sizeof(indices[0]);
    config_index_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    config_index_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    gfx::handle_buffer_t index_buffer = context.create_buffer(config_index_buffer);
    gfx::helper::copy_mappable_buffer(context, index_buffer, indices);

    gfx::config_buffer_t config_vertex_buffer{};        
    config_vertex_buffer.vk_size = vertices.size() * sizeof(vertices[0]);
    config_vertex_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    config_vertex_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    gfx::handle_buffer_t vertex_buffer = context.create_buffer(config_vertex_buffer);
    gfx::helper::copy_mappable_buffer(context, vertex_buffer, vertices);

    gfx::handle_image_t image = gfx::helper::load_image_from_path(context, renderer.command_pool, "../../assets/texture/noise.jpg", VK_FORMAT_R8G8B8A8_SRGB);
    gfx::handle_image_view_t image_view = context.create_image_view({.handle_image = image});

    gfx::handle_descriptor_set_t ds = context.allocate_descriptor_set({ .handle_descriptor_set_layout = dsl });
    context.update_descriptor_set(ds)
        .push_buffer_write(0, { .handle_buffer = index_buffer })
        .push_buffer_write(1, { .handle_buffer = vertex_buffer })
        .push_image_write(2, { .handle_sampler = sampler })
        .push_image_write(3, { .handle_image_view = image_view, .vk_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL })
        .commit();
    
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
        color_rendering_attachment.handle_image_view = o_image_view;

        context.cmd_image_memory_barrier(commandbuffer, o_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        context.cmd_begin_rendering(commandbuffer, { color_rendering_attachment }, std::nullopt, VkRect2D{ VkOffset2D{}, { static_cast<uint32_t>(width), static_cast<uint32_t>(height) } });
        context.cmd_bind_pipeline(commandbuffer, p);
        context.cmd_bind_descriptor_sets(commandbuffer, p, 0, { ds });
        context.cmd_set_viewport_and_scissor(commandbuffer, viewport, scissor);
        context.cmd_draw(commandbuffer, 6, 1, 0, 0);
        context.cmd_end_rendering(commandbuffer);

        context.cmd_image_memory_barrier(commandbuffer, o_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

        renderer.end();
    }

    return 0;
}

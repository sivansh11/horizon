#include "core/core.hpp"
#include "core/window.hpp"
#include "core/random.hpp"
#include "core/model.hpp"

#include "gfx/context.hpp"
#include "gfx/helper.hpp"
#include "gfx/base_renderer.hpp"

#include "display.hpp"
#include "bvh.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

int main() {
    core::log_t::set_log_level(core::log_level_t::e_info);

    core::window_t window{ "rt", 800, 800 };
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
    config_descriptor_set_layout.add_layout_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                .add_layout_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                .add_layout_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    gfx::handle_descriptor_set_layout_t descriptor_set_layout = context.create_descriptor_set_layout(config_descriptor_set_layout);

    gfx::config_pipeline_layout_t config_raytracing_pipeline_layout{};
    config_raytracing_pipeline_layout.add_descriptor_set_layout(descriptor_set_layout);
    gfx::handle_pipeline_layout_t raytracing_pipeline_layout = context.create_pipeline_layout(config_raytracing_pipeline_layout);

    gfx::config_pipeline_t config_raytracing_pipeline{};
    config_raytracing_pipeline.handle_pipeline_layout = raytracing_pipeline_layout;
    config_raytracing_pipeline.add_color_attachment(VK_FORMAT_R8G8B8A8_SRGB, gfx::default_color_blend_attachment())
                              .add_shader(context.create_shader(gfx::config_shader_t{ .code = core::read_file("../../assets/shaders/raytracing/glsl.vert").data(), .name = "raytracing vertex", .type = gfx::shader_type_t::e_vertex }))
                              .add_shader(context.create_shader(gfx::config_shader_t{ .code = core::read_file("../../assets/shaders/raytracing/glsl.frag").data(), .name = "raytracing fragment", .type = gfx::shader_type_t::e_fragment }));
    gfx::handle_pipeline_t raytracing_pipeline = context.create_graphics_pipeline(config_raytracing_pipeline);

    auto model = core::load_model_from_path("../../assets/models/triangle/triangle.obj");
    std::vector<triangle_t> triangles;
    {
        for (auto& mesh : model.meshes) {
            check(mesh.indices.size() % 3 == 0, "indices count is not a multiple of 3");
            for (size_t i = 0; i < mesh.indices.size(); i += 3) {
                core::vertex_t v0 = mesh.vertices[mesh.indices[i + 0]];
                core::vertex_t v1 = mesh.vertices[mesh.indices[i + 1]];
                core::vertex_t v2 = mesh.vertices[mesh.indices[i + 2]];

                triangle_t triangle;
                triangle.p0 = v0.position;
                triangle.p1 = v1.position;
                triangle.p2 = v2.position;

                triangles.push_back(triangle);
            }
        }
    }
    horizon_info("Loaded file with {} triangle(s)", triangles.size());
    horizon_info("Triangles");
    for (auto& triangle : triangles) {
        horizon_info("p0: {} p1: {} p2: {}", glm::to_string(triangle.p0), glm::to_string(triangle.p1), glm::to_string(triangle.p2));
    }

    std::vector<bounding_box_t> aabbs(triangles.size());
    std::vector<glm::vec3> centers(triangles.size());
    
    for (size_t i = 0; i < triangles.size(); i++) {
        aabbs[i] = bounding_box_t{}.extend(triangles[i].p0).extend(triangles[i].p1).extend(triangles[i].p2);
        centers[i] = (triangles[i].p0 + triangles[i].p1 + triangles[i].p2) / 3.0f;
    }

    bvh_t bvh = bvh_t::build(aabbs.data(), centers.data(), triangles.size());

    horizon_info("Built BVH with {} node(s) and depth {}", bvh.nodes.size(), bvh.depth());
    horizon_info("Nodes");
    for (auto& node : bvh.nodes) {
        horizon_info("bb min: {}, max: {}, primitive_count: {}, first_index: {}", glm::to_string(node.bounding_box.min), glm::to_string(node.bounding_box.max), node.primitive_count, node.first_index);
    }

    horizon_info("Primitive Indices");
    for (auto& id : bvh.primitive_indices) {
        horizon_info("{}", id);
    }

    gfx::config_buffer_t config_triangle_buffer{};
    config_triangle_buffer.vk_size = sizeof(triangle_t) * triangles.size();
    config_triangle_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    config_triangle_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    gfx::handle_buffer_t triangle_buffer = context.create_buffer(config_triangle_buffer);
    std::memcpy(context.map_buffer(triangle_buffer), triangles.data(), sizeof(triangle_t) * triangles.size());

    gfx::config_buffer_t config_node_buffer{};
    config_node_buffer.vk_size = sizeof(node_t) * bvh.nodes.size();
    config_node_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    config_node_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    gfx::handle_buffer_t node_buffer = context.create_buffer(config_node_buffer);
    std::memcpy(context.map_buffer(node_buffer), bvh.nodes.data(), sizeof(node_t) * bvh.nodes.size());

    gfx::config_buffer_t config_primitive_index_buffer{};
    config_primitive_index_buffer.vk_size = sizeof(uint32_t) * bvh.primitive_indices.size();
    config_primitive_index_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    config_primitive_index_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    gfx::handle_buffer_t primitive_index_buffer = context.create_buffer(config_primitive_index_buffer);
    std::memcpy(context.map_buffer(primitive_index_buffer), bvh.primitive_indices.data(), sizeof(uint32_t) * bvh.primitive_indices.size());
    
    gfx::handle_descriptor_set_t descriptor_set = context.allocate_descriptor_set({ .handle_descriptor_set_layout = descriptor_set_layout });
    context.update_descriptor_set(descriptor_set)
        .push_buffer_write(0, gfx::buffer_descriptor_info_t{ .handle_buffer = triangle_buffer })
        .push_buffer_write(1, gfx::buffer_descriptor_info_t{ .handle_buffer = node_buffer })
        .push_buffer_write(2, gfx::buffer_descriptor_info_t{ .handle_buffer = primitive_index_buffer })
        .commit();

    core::frame_timer_t frame_timer{ 60.f };
    while (!window.should_close()) {
        core::clear_frame_function_times();
        core::window_t::poll_events();
        if (glfwGetKey(window.window(), GLFW_KEY_ESCAPE)) break;

        renderer.begin();

        auto [viewport, scissor] = gfx::helper::fill_viewport_and_scissor_structs(width, height);

        gfx::handle_commandbuffer_t commandbuffer = renderer.current_commandbuffer();
        {
            gfx::rendering_attachment_t rendering_attachment{};
            rendering_attachment.clear_value = { 0, 0, 0, 0 };
            rendering_attachment.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            rendering_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
            rendering_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
            rendering_attachment.handle_image_view = target_image_view;

            context.cmd_image_memory_barrier(commandbuffer, target_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
            context.cmd_begin_rendering(commandbuffer, { rendering_attachment }, std::nullopt, VkRect2D{ VkOffset2D{}, { static_cast<uint32_t>(width), static_cast<uint32_t>(height) } });
            context.cmd_bind_pipeline(commandbuffer, raytracing_pipeline);
            context.cmd_set_viewport_and_scissor(commandbuffer, viewport, scissor);
            context.cmd_bind_descriptor_sets(commandbuffer, raytracing_pipeline, 0, { descriptor_set });
            context.cmd_draw(commandbuffer, 6, 1, 0, 0);
            context.cmd_end_rendering(commandbuffer);
            context.cmd_image_memory_barrier(commandbuffer, target_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        }

        renderer.end();
    }

    context.wait_idle();

    return 0;
}
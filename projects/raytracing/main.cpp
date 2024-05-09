#include "core/core.hpp"
#include "core/window.hpp"
#include "core/random.hpp"
#include "core/model.hpp"

#include "gfx/context.hpp"
#include "gfx/helper.hpp"
#include "gfx/base_renderer.hpp"

#include "utility.hpp"
#include "display.hpp"
#include "bvh.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

struct gpu_mesh_t {
    gfx::handle_buffer_t vertex_buffer;
    gfx::handle_buffer_t index_buffer;

    size_t vertex_count;
    size_t index_count;
};

struct model_ubo_t {
    glm::mat4 model;
    glm::mat4 inv_model;
};

struct camera_ubo_t {
    glm::mat4 projection;
    glm::mat4 inv_projection;
    glm::mat4 view;
    glm::mat4 inv_view;
};

struct object_t {
    gfx::handle_buffer_t model_buffer;
    gfx::handle_buffer_t color_buffer;
    gfx::handle_descriptor_set_t object_descriptor_set;
};

object_t new_object(gfx::context_t& context, gfx::handle_descriptor_set_layout_t descriptor_set_layout, core::rng_t& rng, const transform_t& transform) {
    object_t object{};

    gfx::config_buffer_t config_model_buffer{};
    config_model_buffer.vk_size = sizeof(model_ubo_t);
    config_model_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    config_model_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    object.model_buffer = context.create_buffer(config_model_buffer);

    gfx::config_buffer_t config_color_buffer{};
    config_color_buffer.vk_size = sizeof(glm::vec3);
    config_color_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    config_color_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    object.color_buffer = context.create_buffer(config_color_buffer);

    model_ubo_t model_ubo{};
    model_ubo.model = transform.mat4();
    model_ubo.inv_model = glm::inverse(model_ubo.model);
    std::memcpy(context.map_buffer(object.model_buffer), &model_ubo, sizeof(model_ubo_t));

    glm::vec3 color = rng.random3f() / 100.f;
    std::memcpy(context.map_buffer(object.color_buffer), &color, sizeof(glm::vec3));

    object.object_descriptor_set = context.allocate_descriptor_set({ .handle_descriptor_set_layout = descriptor_set_layout });
    context.update_descriptor_set(object.object_descriptor_set)
        .push_buffer_write(0, { .handle_buffer = object.model_buffer })
        .push_buffer_write(1, { .handle_buffer = object.color_buffer })
        .commit();

    return object;
}

int main() {
    core::log_t::set_log_level(core::log_level_t::e_info);

    core::window_t window{ "raytracing", 800, 800 };
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

    gfx::config_descriptor_set_layout_t config_raytracing_inputs_descriptor_set_layout{};
    config_raytracing_inputs_descriptor_set_layout.add_layout_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                                  .add_layout_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                                  .add_layout_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    gfx::handle_descriptor_set_layout_t raytracing_inputs_descriptor_set_layout = context.create_descriptor_set_layout(config_raytracing_inputs_descriptor_set_layout);

    gfx::config_descriptor_set_layout_t config_camera_inputs_descriptor_set_layout{};
    config_camera_inputs_descriptor_set_layout.add_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    gfx::handle_descriptor_set_layout_t camera_inputs_descriptor_set_layout = context.create_descriptor_set_layout(config_camera_inputs_descriptor_set_layout);

    gfx::config_pipeline_layout_t config_raytracing_pipeline_layout{};
    config_raytracing_pipeline_layout.add_descriptor_set_layout(raytracing_inputs_descriptor_set_layout)
                                     .add_descriptor_set_layout(camera_inputs_descriptor_set_layout);
    gfx::handle_pipeline_layout_t raytracing_pipeline_layout = context.create_pipeline_layout(config_raytracing_pipeline_layout);    

    gfx::config_pipeline_t config_raytracing_pipeline{};
    config_raytracing_pipeline.handle_pipeline_layout = raytracing_pipeline_layout;
    config_raytracing_pipeline.add_color_attachment(VK_FORMAT_R8G8B8A8_SRGB, gfx::default_color_blend_attachment())
                                    .add_shader(context.create_shader(gfx::config_shader_t{ .code = core::read_file("../../assets/shaders/raytracing/glsl.vert").data(), .name = "raytracing vertex", .type = gfx::shader_type_t::e_vertex }))
                                    .add_shader(context.create_shader(gfx::config_shader_t{ .code = core::read_file("../../assets/shaders/raytracing/glsl.frag").data(), .name = "raytracing fragment", .type = gfx::shader_type_t::e_fragment }));
    gfx::handle_pipeline_t raytracing_pipeline = context.create_graphics_pipeline(config_raytracing_pipeline);

    core::model_t model = core::load_model_from_path("../../assets/models/adam_thing/adamHead.gltf");
    std::vector<triangle_t> triangles;
    std::vector<bounding_box_t> bounding_boxes;
    std::vector<glm::vec3> centers;
    for (auto& mesh : model.meshes) {
        check(mesh.indices.size() % 3 == 0, "mesh indices not a multiple of 3");
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
    bounding_boxes.resize(triangles.size());
    centers.resize(triangles.size());
    for (size_t i = 0; i < triangles.size(); i++) {
        bounding_boxes[i] = bounding_box_t{}.extend(triangles[i].p0).extend(triangles[i].p1).extend(triangles[i].p2);
        centers[i] = (triangles[i].p0 + triangles[i].p1 + triangles[i].p2) / 3.0f;
    }
    bvh_t bvh = bvh_t::build(bounding_boxes.data(), centers.data(), triangles.size());
    horizon_info("Built blas with {} nodes and depth {}", bvh.nodes.size(), bvh.depth());

    renderer::handle_buffer_t triangles_buffer = renderer::create_and_push_vector(renderer, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, {}, triangles);
    renderer::handle_buffer_t nodes_buffer = renderer::create_and_push_vector(renderer, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, {}, bvh.nodes);
    renderer::handle_buffer_t primitive_indices_buffer = renderer::create_and_push_vector(renderer, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, {}, bvh.primitive_indices);

    auto raytracing_inputs_descriptor_set = renderer.allocate_descriptor_set(renderer::resource_policy_t::e_sparse, { .handle_descriptor_set_layout = raytracing_inputs_descriptor_set_layout });
    renderer.update_descriptor_set(raytracing_inputs_descriptor_set)
            .push_buffer_write(0, { .handle_buffer = triangles_buffer })
            .push_buffer_write(1, { .handle_buffer = nodes_buffer })
            .push_buffer_write(2, { .handle_buffer = primitive_indices_buffer })
            .commit();

    gfx::config_buffer_t config_camera_inputs_buffer{};
    config_camera_inputs_buffer.vk_size = sizeof(camera_ubo_t);
    config_camera_inputs_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    config_camera_inputs_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    renderer::handle_buffer_t camera_inputs_buffer = renderer.create_buffer(renderer::resource_policy_t::e_every_frame, config_camera_inputs_buffer);

    auto camera_inputs_descriptor_set = renderer.allocate_descriptor_set(renderer::resource_policy_t::e_every_frame, { .handle_descriptor_set_layout = camera_inputs_descriptor_set_layout });
    renderer.update_descriptor_set(camera_inputs_descriptor_set)
            .push_buffer_write(0, { .handle_buffer = camera_inputs_buffer })
            .commit();

    editor_camera_t editor_camera{ window };
    core::frame_timer_t frame_timer{ 60.f };

    gfx::handle_timer_t timer = context.create_timer({});

    while (!window.should_close()) {
        core::clear_frame_function_times();
        core::window_t::poll_events();
        if (glfwGetKey(window.window(), GLFW_KEY_ESCAPE)) break;

        auto dt = frame_timer.update();
        editor_camera.update(dt.count());

        renderer.begin();
        auto commandbuffer = renderer.current_commandbuffer();
        auto [viewport, scissor] = gfx::helper::fill_viewport_and_scissor_structs(width, height);

        camera_ubo_t camera_ubo{};
        camera_ubo.projection = editor_camera.projection();
        camera_ubo.inv_projection = glm::inverse(camera_ubo.projection);
        camera_ubo.view = editor_camera.view();
        camera_ubo.inv_view = glm::inverse(camera_ubo.view);
        std::memcpy(context.map_buffer(renderer.buffer(camera_inputs_buffer)), &camera_ubo, sizeof(camera_ubo_t));
    
        gfx::rendering_attachment_t rendering_attachment{};
        rendering_attachment.clear_value = {0, 0, 0, 0};
        rendering_attachment.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        rendering_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        rendering_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
        rendering_attachment.handle_image_view = target_image_view;

        // raytracing

        context.cmd_image_memory_barrier(commandbuffer, target_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        context.cmd_begin_timer(commandbuffer, timer);
        context.cmd_begin_rendering(commandbuffer, { rendering_attachment }, std::nullopt, VkRect2D{ VkOffset2D{}, { static_cast<uint32_t>(width), static_cast<uint32_t>(height) } });
        context.cmd_bind_pipeline(commandbuffer, raytracing_pipeline);
        context.cmd_set_viewport_and_scissor(commandbuffer, viewport, scissor);
        context.cmd_bind_descriptor_sets(commandbuffer, raytracing_pipeline, 0, { renderer.descriptor_set(raytracing_inputs_descriptor_set), renderer.descriptor_set(camera_inputs_descriptor_set) });
        context.cmd_draw(commandbuffer, 6, 1, 0, 0);
        context.cmd_end_timer(commandbuffer, timer);

        if (auto t = context.timer_get_time(timer)) {
            horizon_info("{}", *t);
        } else {
            horizon_info("");
        }

        context.cmd_end_rendering(commandbuffer);
        context.cmd_image_memory_barrier(commandbuffer, target_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

        renderer.end();
    }

    context.wait_idle();

    return 0;
}
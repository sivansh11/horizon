#include "test.hpp"

#define horizon_profile_enable
#include "core/core.hpp"
#include "core/window.hpp"

#include "gfx/context.hpp"

#include "gfx/base_renderer.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "utility.hpp"

struct camera_t {
    glm::mat4 projection;
    glm::mat4 inv_projection;
    glm::mat4 view;
    glm::mat4 inv_view;
};

struct object_t {
    glm::mat4 model;
    glm::mat4 inv_model;
};

int main() {
    core::window_t window{ "diffuse", 1200, 800 };
    gfx::context_t context{ true };

    auto [width, height] = window.dimensions();

    gfx::config_image_t config_final_image{};
    config_final_image.vk_width = width;
    config_final_image.vk_height = height;
    config_final_image.vk_depth = 1;
    config_final_image.vk_type = VK_IMAGE_TYPE_2D;
    config_final_image.vk_format = VK_FORMAT_R8G8B8A8_SRGB;
    config_final_image.vk_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    config_final_image.vk_mips = 1;
    auto final_image = context.create_image(config_final_image);
    gfx::handle_image_view_t final_image_view = context.create_image_view({.handle_image = final_image});

    gfx::config_image_t config_depth_image{};
    config_depth_image.vk_width = width;
    config_depth_image.vk_height = height;
    config_depth_image.vk_depth = 1;
    config_depth_image.vk_type = VK_IMAGE_TYPE_2D;
    config_depth_image.vk_format = VK_FORMAT_D32_SFLOAT;
    config_depth_image.vk_usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    config_depth_image.vk_mips = 1;
    auto depth_image = context.create_image(config_depth_image);
    gfx::handle_image_view_t depth_image_view = context.create_image_view({.handle_image = depth_image});

    gfx::handle_sampler_t sampler = context.create_sampler({});

    renderer::base_renderer_t renderer{ window, context, sampler, final_image_view };

    create_material_descriptor_set_layout(context);

    auto gpu_meshes = load_model_from_path(context, renderer.command_pool, "../../assets/models/Sponza/glTF/Sponza.gltf");

    gfx::config_descriptor_set_layout_t camera_config_descriptor_set_layout{};
    camera_config_descriptor_set_layout.add_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS);
    auto camera_descriptor_set_layout = context.create_descriptor_set_layout(camera_config_descriptor_set_layout);

    gfx::config_descriptor_set_layout_t object_config_descriptor_set_layout{};
    object_config_descriptor_set_layout.add_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS);
    auto object_descriptor_set_layout = context.create_descriptor_set_layout(object_config_descriptor_set_layout);
    
    gfx::config_pipeline_layout_t config_pipeline_layout{};
    config_pipeline_layout.add_descriptor_set_layout(camera_descriptor_set_layout)  
                                  .add_descriptor_set_layout(object_descriptor_set_layout)
                                  .add_descriptor_set_layout(material_descriptor_set_layout);
    auto pipeline_layout = context.create_pipeline_layout(config_pipeline_layout);

    gfx::config_pipeline_t diffuse_config_pipeline{};
    diffuse_config_pipeline.handle_pipeline_layout = pipeline_layout;
    diffuse_config_pipeline.add_color_attachment(VK_FORMAT_R8G8B8A8_SRGB, gfx::default_color_blend_attachment())
                           .set_depth_attachment(VK_FORMAT_D32_SFLOAT, VkPipelineDepthStencilStateCreateInfo{
                                .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                                .depthTestEnable = VK_TRUE,
                                .depthWriteEnable = VK_TRUE,
                                .depthCompareOp = VK_COMPARE_OP_LESS,
                                .depthBoundsTestEnable = VK_FALSE,
                                .stencilTestEnable = VK_FALSE,
                           })
                           .add_vertex_input_binding_description(0, sizeof(core::vertex_t), VK_VERTEX_INPUT_RATE_VERTEX)
                           .add_vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, position))
                           .add_vertex_input_attribute_description(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, normal))
                           .add_vertex_input_attribute_description(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(core::vertex_t, uv))
                           .add_vertex_input_attribute_description(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, tangent))
                           .add_vertex_input_attribute_description(0, 4, VK_FORMAT_R32G32B32_SFLOAT, offsetof(core::vertex_t, bi_tangent))
                           .add_shader(context.create_shader(gfx::config_shader_t{ .code = core::read_file("../../assets/shaders/diffuse_only/glsl.vert").data(), .name = "diffuse only vertex", .type = gfx::shader_type_t::e_vertex}))
                           .add_shader(context.create_shader(gfx::config_shader_t{ .code = core::read_file("../../assets/shaders/diffuse_only/glsl.frag").data(), .name = "diffuse only fragment", .type = gfx::shader_type_t::e_fragment}));
    auto diffuse_pipeline = context.create_graphics_pipeline(diffuse_config_pipeline);
    
    gfx::config_buffer_t config_camera_buffer{};
    config_camera_buffer.vk_size = sizeof(camera_t);
    config_camera_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    config_camera_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    renderer::handle_buffer_t camera_buffer = renderer.create_buffer(renderer::resource_policy_t::e_every_frame, config_camera_buffer);

    renderer::handle_descriptor_set_t camera_descriptor_set = renderer.allocate_descriptor_set(renderer::resource_policy_t::e_every_frame, gfx::config_descriptor_set_t{ .handle_descriptor_set_layout = camera_descriptor_set_layout });
    renderer.update_descriptor_set(camera_descriptor_set)
        .push_buffer_write(0, renderer::buffer_descriptor_info_t{ .handle_buffer = camera_buffer })
        .commit();

    gfx::config_buffer_t config_object_buffer{};
    config_object_buffer.vk_size = sizeof(object_t);
    config_object_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    config_object_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    gfx::handle_buffer_t object_buffer = context.create_buffer(config_object_buffer);
    gfx::handle_descriptor_set_t object_descriptor_set = context.allocate_descriptor_set(gfx::config_descriptor_set_t{.handle_descriptor_set_layout = object_descriptor_set_layout});
    context.update_descriptor_set(object_descriptor_set)
        .push_buffer_write(0, {.handle_buffer = object_buffer})
        .commit();

    auto *object = reinterpret_cast<object_t *>(context.map_buffer(object_buffer));
    object->model = {1.f};
    object->inv_model = glm::inverse(object->model);

    auto [viewport, scissor] = gfx::helper::fill_viewport_and_scissor_structs(width, height);

    editor_camera_t editor_camera{ window };
    core::frame_timer_t frame_timer{ 60.f };
    
    while (!window.should_close()) {
        core::clear_frame_function_times();

        core::window_t::poll_events();

        auto dt = frame_timer.update();
        editor_camera.update(dt.count());
        std::cout << dt.count() << '\n';

        if (glfwGetKey(window.window(), GLFW_KEY_ESCAPE)) break;

        renderer.begin();
        auto commandbuffer = renderer.current_commandbuffer();

        // update camera
        auto camera = reinterpret_cast<camera_t *>(context.map_buffer(renderer.buffer(camera_buffer)));
        camera->projection = editor_camera.projection();
        camera->inv_projection = glm::inverse(camera->projection);
        camera->view = editor_camera.view();
        camera->inv_view = glm::inverse(camera->view);

        gfx::rendering_attachment_t color_rendering_attachment{};
        color_rendering_attachment.clear_value = {0, 0, 0, 0};
        color_rendering_attachment.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_rendering_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_rendering_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
        color_rendering_attachment.handle_image_view = final_image_view;

        gfx::rendering_attachment_t depth_rendering_attachment{};
        depth_rendering_attachment.clear_value.depthStencil.depth = 1;
        depth_rendering_attachment.image_layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depth_rendering_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_rendering_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
        depth_rendering_attachment.handle_image_view = depth_image_view;

        context.cmd_image_memory_barrier(commandbuffer, final_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        context.cmd_image_memory_barrier(commandbuffer, depth_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
        context.cmd_begin_rendering(commandbuffer, { color_rendering_attachment }, depth_rendering_attachment, VkRect2D{VkOffset2D{}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}});
        context.cmd_bind_pipeline(commandbuffer, diffuse_pipeline);
        context.cmd_set_viewport_and_scissor(commandbuffer, viewport, scissor);
        context.cmd_bind_descriptor_sets(commandbuffer, diffuse_pipeline, 0, { renderer.descriptor_set(camera_descriptor_set), object_descriptor_set  });
        for (auto mesh : gpu_meshes) {
            context.cmd_bind_descriptor_sets(commandbuffer, diffuse_pipeline, 2, { mesh.material_descriptor_set });
            context.cmd_bind_vertex_buffers(commandbuffer, 0, { mesh.vertex_buffer }, { 0 });
            context.cmd_bind_index_buffer(commandbuffer, mesh.index_buffer, 0, VK_INDEX_TYPE_UINT32);
            context.cmd_draw_indexed(commandbuffer, mesh.index_count, 1, 0, 0, 0);
        }
        context.cmd_end_rendering(commandbuffer);
        context.cmd_image_memory_barrier(commandbuffer, final_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

        renderer.end();
    }

    context.wait_idle();
    destroy_material_descriptor_set_layout(context);

    return 0;
}
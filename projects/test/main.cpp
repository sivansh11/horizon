#include "test.hpp"

#define horizon_profile_enable
#include "core/core.hpp"
#include "core/window.hpp"

#include "gfx/context.hpp"

#include "gfx/base_renderer.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

static const char *test_vertex = R"(
#version 450

layout(location = 0) out vec3 fragColor;

vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}
)";

static const char *test_fragment = R"(
#version 450

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(1, 1, 1, 1.0);
}
)";

static const char *compute_shader_code = R"(
#version 450
#extension GL_EXT_scalar_block_layout : enable

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout (scalar, binding = 0) buffer ssbo {
    int data;
};

layout (push_constant) uniform push {
    int val;
};

void main() {
    data = val;
}
)";


void test_compute() {
    gfx::context_t context{ true };

    auto shader = context.create_shader(gfx::config_shader_t{ .code = compute_shader_code, .name = "compute", .type = gfx::shader_type_t::e_compute });

    gfx::config_descriptor_set_layout_t config_descriptor_set_layout{};
    config_descriptor_set_layout.add_layout_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    auto descriptor_set_layout = context.create_descriptor_set_layout(config_descriptor_set_layout);

    gfx::config_pipeline_layout_t config_pipeline_layout{};
    config_pipeline_layout.add_descriptor_set_layout(descriptor_set_layout);
    config_pipeline_layout.add_push_constant(sizeof(int), VK_SHADER_STAGE_COMPUTE_BIT);
    auto pipeline_layout = context.create_pipeline_layout(config_pipeline_layout);
    gfx::config_pipeline_t config_pipeline{};
    config_pipeline.pipeline_layout = pipeline_layout;
    config_pipeline.add_shader(shader);
    auto pipeline = context.create_compute_pipeline(config_pipeline);

    auto descriptor_set = context.allocate_descriptor_set({descriptor_set_layout});

    gfx::config_buffer_t config_buffer{};
    config_buffer.vk_size = sizeof(int);
    config_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    config_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    auto buffer = context.create_buffer(config_buffer);

    context.update_descriptor_set(descriptor_set).push_buffer_write(0, gfx::buffer_descriptor_info_t{ .buffer = buffer, .vk_offset = 0, .vk_range = VK_WHOLE_SIZE })
                                                 .commit();

    auto command_pool = context.create_command_pool({});
    auto commandbuffer = context.allocate_commandbuffer({.command_pool = command_pool});

    int i = 5;

    context.begin_commandbuffer(commandbuffer, true);
    context.cmd_bind_pipeliine(commandbuffer, pipeline);
    context.cmd_bind_descriptor_sets(commandbuffer, pipeline, 0, { descriptor_set });
    context.cmd_push_constants(commandbuffer, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int), &i);
    context.cmd_dispatch(commandbuffer, 1, 1, 1);
    context.end_commandbuffer(commandbuffer);

    auto fence = context.create_fence({});
    context.reset_fence(fence);
    context.submit_commandbuffer(commandbuffer, {}, {}, {}, fence);
    context.wait_fence(fence);

    int *buffer_i = reinterpret_cast<int *>(context.map_buffer(buffer));

    horizon_info("value of i: {}", *buffer_i);
}

void test_graphics() {
    core::window_t window{ "test", 640, 420 };
    gfx::context_t context{ true };
    auto swapchain = context.create_swapchain(window);

    // {
    //     gfx::config_image_t config_image{};
    //     config_image.vk_width = 640;
    //     config_image.vk_height = 420;
    //     config_image.vk_depth = 1;
    //     config_image.vk_type = VK_IMAGE_TYPE_2D;
    //     config_image.vk_format = VK_FORMAT_R8G8B8A8_SRGB;
    //     config_image.vk_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    //     config_image.vk_mips = 1;
    //     auto image = context.create_image(config_image);

    //     auto image_view = context.create_image_view({.image = image});

    //     auto sampler = context.create_sampler({});

    //     gfx::config_descriptor_set_layout_t cdsl{};
    //     cdsl.add_layout_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL_GRAPHICS);
    //     auto dsl = context.create_descriptor_set_layout(cdsl);

    //     auto ds = context.allocate_descriptor_set({.descriptor_set_layout = dsl});

    //     context.update_descriptor_set(ds).push_image_write(0, gfx::image_descriptor_info_t{ .handle_sampler = sampler, .handle_image_view = image_view, .vk_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }).commit();
    // }

    gfx::config_pipeline_layout_t config_pipeline_layout{};
    auto pipeline_layout = context.create_pipeline_layout(config_pipeline_layout);
    gfx::config_pipeline_t config_pipeline{};
    gfx::config_shader_t config_shader{};
    config_shader.code = test_vertex;
    config_shader.name = "test_vertex";
    config_shader.type = gfx::shader_type_t::e_vertex;
    auto vertex_shader = context.create_shader(config_shader);
    config_shader.code = test_fragment;
    config_shader.name = "test_fragment";
    config_shader.type = gfx::shader_type_t::e_fragment;
    auto fragment_shader = context.create_shader(config_shader);
    config_pipeline.add_shader(vertex_shader);
    config_pipeline.add_shader(fragment_shader);
    config_pipeline.add_color_attachment(VK_FORMAT_B8G8R8A8_SRGB, gfx::default_color_blend_attachment());
    config_pipeline.pipeline_layout = pipeline_layout;
    auto pipeline = context.create_graphics_pipeline(config_pipeline);

    auto in_flight_fence = context.create_fence({});
    auto image_available_semaphore = context.create_semaphore({});
    auto render_finished_semaphore = context.create_semaphore({});

    auto command_pool = context.create_command_pool({});
    auto commandbuffer = context.allocate_commandbuffer({ .command_pool = command_pool });

    VkViewport swapchain_viewport{};
    swapchain_viewport.x = 0;
    swapchain_viewport.y = 0;
    swapchain_viewport.width = 640;
    swapchain_viewport.height = 420;
    swapchain_viewport.minDepth = 0;
    swapchain_viewport.maxDepth = 1;
    VkRect2D swapchain_scissor{};
    swapchain_scissor.offset = {0, 0};
    swapchain_scissor.extent = {640, 420};

    while (!window.should_close()) {
        core::window_t::poll_events();

        context.wait_fence(in_flight_fence);
        
        auto next_image = context.get_swapchain_next_image_index(swapchain, image_available_semaphore, null_handle);
        check(next_image, "Failed to get next image");
        context.reset_fence(in_flight_fence);

        context.begin_commandbuffer(commandbuffer);

        context.cmd_image_memory_barrier(commandbuffer, 
                                         context.get_swapchain_images(swapchain)[*next_image],
                                         VK_IMAGE_LAYOUT_UNDEFINED, 
                                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
                                         0, 
                                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 
                                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        gfx::rendering_attachment_t color_rendering_attachment{};
        color_rendering_attachment.clear_value = {0, 0, 0, 0};
        color_rendering_attachment.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_rendering_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_rendering_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
        color_rendering_attachment.handle_image_view = context.get_swapchain_image_views(swapchain)[*next_image];
        context.cmd_begin_rendering(commandbuffer, {color_rendering_attachment}, std::nullopt, VkRect2D{VkOffset2D{}, {640, 420}});
        context.cmd_bind_pipeliine(commandbuffer, pipeline);
        context.cmd_set_viewport_and_scissor(commandbuffer, swapchain_viewport, swapchain_scissor);
        context.cmd_draw(commandbuffer, 6, 1, 0, 0);
        context.cmd_end_rendering(commandbuffer);
        context.cmd_image_memory_barrier(commandbuffer, 
                                         context.get_swapchain_images(swapchain)[*next_image],
                                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
                                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 
                                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 
                                         0, 
                                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        context.end_commandbuffer(commandbuffer);

        context.submit_commandbuffer(commandbuffer, {image_available_semaphore}, {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}, {render_finished_semaphore}, in_flight_fence);
        context.present_swapchain(swapchain, *next_image, {render_finished_semaphore});
    }
}

int main() {
    test_compute();

    test_graphics();

    return 0;
}
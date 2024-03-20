#include "test.hpp"

#define horizon_profile_enable
#include "core/core.hpp"
#include "core/window.hpp"

#include "gfx/new_context.hpp"

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

int main() {
    gfx::new_context_t context{ true };

    gfx::config_buffer_t config_buffer{};
    config_buffer.vk_size = sizeof(int);
    config_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    config_buffer.vma_allocation_create_flags = VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    gfx::handle_buffer_t buffer = context.create_buffer(config_buffer);

    gfx::handle_pipeline_layout_t pipeline_layout = context.create_pipeline_layout({});

    gfx::config_pipeline_t config_pipeline{};
    config_pipeline.pipeline_layout = pipeline_layout;
    config_pipeline.add_shader(context.create_shader(gfx::config_shader_t{ .code = test_vertex, .name = "vertex", .type = gfx::shader_type_t::e_vertex }));
    config_pipeline.add_shader(context.create_shader(gfx::config_shader_t{ .code = test_fragment, .name = "fragment", .type = gfx::shader_type_t::e_fragment }));
    config_pipeline.add_color_attachment(VK_FORMAT_B8G8R8A8_SRGB, gfx::default_color_blend_attachment());
    gfx::handle_pipeline_t pipeline = context.create_graphics_pipeline(config_pipeline);

    gfx::handle_fence_t fence = context.create_fence({});
    // context.destroy_fence(fence);

    gfx::handle_command_pool_t command_pool = context.create_command_pool({});
    // context.destroy_command_pool(command_pool);

    gfx::handle_commandbuffer_t commandbuffer = context.allocate_commandbuffer({ command_pool });
    // context.free_commandbuffer(commandbuffer);

    // context.reset_fence(fence);
    // context.begin_commandbuffer(commandbuffer, true);
    // context.cmd_bind_pipeliine(commandbuffer, pipeline);
    // context.cmd_bind_descriptor_sets(commandbuffer, pipeline, 0, { descriptor_set });
    // int i = 69;
    // context.cmd_push_constants(commandbuffer, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int), &i);
    // context.cmd_dispatch(commandbuffer, 1, 1, 1);
    // context.end_commandbuffer(commandbuffer);        
    // context.submit_commandbuffer(commandbuffer, {}, {}, {}, fence);
    // context.wait_fence(fence);
    
    // context.reset_fence(fence);
    // context.begin_commandbuffer(commandbuffer, true);

    // // gfx::rendering_attachment_t color_rendering_attachment{};
    // // color_rendering_attachment.
    // // context.cmd_image_memory_barrier(commandbuffer, {})

    // // context.end_commandbuffer(commandbuffer);        
    // context.submit_commandbuffer(commandbuffer, {}, {}, {}, fence);
    // context.wait_fence(fence);
    

    int *p = (int *)context.map_buffer(buffer);
    horizon_info("{}", *p);

    gfx::config_image_t config_image{};
    config_image.vk_width = 600;
    config_image.vk_height = 400;
    config_image.vk_depth = 1;
    config_image.vk_type = VK_IMAGE_TYPE_2D;
    config_image.vk_format = VK_FORMAT_R8G8B8A8_SRGB;
    config_image.vk_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;    
    gfx::handle_image_t image = context.create_image(config_image);

    gfx::config_image_view_t config_image_view{};
    config_image_view.image = image;
    gfx::handle_image_view_t image_view = context.create_image_view(config_image_view);

    core::window_t window{ "test", 600, 400 };

    gfx::handle_swapchain_t swapchain = context.create_swapchain({ .window = window });

    while (!window.should_close()) {
        core::window_t::poll_events();
        if (glfwGetKey(window.window(), GLFW_KEY_ESCAPE) == GLFW_PRESS) break;
    }

    return 0;
}
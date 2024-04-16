#include "test.hpp"

#define horizon_profile_enable
#include "core/core.hpp"
#include "core/window.hpp"

#include "gfx/context.hpp"
#include "gfx/base_renderer.hpp"

#include "rendergraph.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <functional>
#include <iostream>

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

layout(set = 0, binding = 0) uniform somthing_t {
    float col_mul;
};

void main() {
    outColor = vec4(fragColor, 1.0) * col_mul;
}
)";

static const char *swapchain_vertex = R"(
#version 450

layout (location = 0) out vec2 out_uv;

vec2 positions[6] = vec2[](
    vec2(-1, -1),
    vec2(-1,  1),
    vec2( 1,  1),
    vec2(-1, -1),
    vec2( 1,  1),
    vec2( 1, -1)
);

vec2 uv[6] = vec2[](
    // vec2(0, 1),
    // vec2(0, 0),
    // vec2(1, 0),
    // vec2(0, 1),
    // vec2(1, 0),
    // vec2(1, 1)
    vec2(0, 0),
    vec2(0, 1),
    vec2(1, 1),
    vec2(0, 0),
    vec2(1, 1),
    vec2(1, 0)
);

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0, 1);
    out_uv = uv[gl_VertexIndex];
}
)";

static const char *swapchain_fragment = R"(
#version 450

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform sampler2D screen;

void main() {
    outColor = texture(screen, uv);
}
)";

int main() {
    core::window_t window{ "test", 640, 420 };
    renderer::base_renderer_t renderer{ window };
    
    gfx::config_descriptor_set_layout_t config_descriptor_set_layout{};
    config_descriptor_set_layout.add_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    auto descriptor_set_layout = renderer.context.create_descriptor_set_layout(config_descriptor_set_layout);

    gfx::config_pipeline_layout_t config_pipeline_layout{};
    config_pipeline_layout.add_descriptor_set_layout(descriptor_set_layout);
    auto pipeline_layout = renderer.context.create_pipeline_layout(config_pipeline_layout);
    gfx::config_pipeline_t config_pipeline{};
    auto vertex_shader = renderer.context.create_shader(gfx::config_shader_t{.code = test_vertex, .name = "vertex_shader", .type = gfx::shader_type_t::e_vertex});
    auto fragment_shader = renderer.context.create_shader(gfx::config_shader_t{.code = test_fragment, .name = "fragment_shader", .type = gfx::shader_type_t::e_fragment});
    config_pipeline.add_shader(vertex_shader);
    config_pipeline.add_shader(fragment_shader);
    config_pipeline.add_color_attachment(VK_FORMAT_R8G8B8A8_SRGB, gfx::default_color_blend_attachment());
    config_pipeline.handle_pipeline_layout = pipeline_layout;
    auto pipeline = renderer.context.create_graphics_pipeline(config_pipeline);

    gfx::config_buffer_t config_buffer{};
    config_buffer.vk_size = sizeof(float);
    config_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    config_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    auto buffer = renderer.create_buffer(renderer::resource_policy_t::e_every_frame, config_buffer);
    auto descriptor_set = renderer.allocate_descriptor_set(renderer::resource_policy_t::e_every_frame, { .handle_descriptor_set_layout = descriptor_set_layout });
    renderer.update_descriptor_set(descriptor_set).push_buffer_write(0, renderer::buffer_descriptor_info_t{ .handle_buffer = buffer }).commit();

    gfx::config_descriptor_set_layout_t config_swapchain_descriptor_set_layout{};
    config_swapchain_descriptor_set_layout.add_layout_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    auto swapchain_descriptor_set_layout = renderer.context.create_descriptor_set_layout(config_swapchain_descriptor_set_layout);

    gfx::config_pipeline_layout_t swapchain_config_pipeline_layout{};
    swapchain_config_pipeline_layout.add_descriptor_set_layout(swapchain_descriptor_set_layout);
    auto swapchain_pipeline_layout = renderer.context.create_pipeline_layout(swapchain_config_pipeline_layout);
    gfx::config_pipeline_t swapchain_config_pipeline{};
    auto swapchain_vertex_shader = renderer.context.create_shader(gfx::config_shader_t{.code = swapchain_vertex, .name = "vertex_shader", .type = gfx::shader_type_t::e_vertex});
    auto swapchain_fragment_shader = renderer.context.create_shader(gfx::config_shader_t{.code = swapchain_fragment, .name = "fragment_shader", .type = gfx::shader_type_t::e_fragment});
    swapchain_config_pipeline.add_shader(swapchain_vertex_shader);
    swapchain_config_pipeline.add_shader(swapchain_fragment_shader);
    swapchain_config_pipeline.add_color_attachment(VK_FORMAT_B8G8R8A8_SRGB, gfx::default_color_blend_attachment());
    swapchain_config_pipeline.handle_pipeline_layout = swapchain_pipeline_layout;
    auto swapchain_pipeline = renderer.context.create_graphics_pipeline(swapchain_config_pipeline);

    gfx::config_image_t config_image{};
    config_image.vk_width = 640;
    config_image.vk_height = 420;
    config_image.vk_depth = 1;
    config_image.vk_type = VK_IMAGE_TYPE_2D;
    config_image.vk_format = VK_FORMAT_R8G8B8A8_SRGB;
    config_image.vk_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    config_image.vk_mips = 1;
    auto image = renderer.context.create_image(config_image);
    gfx::config_image_view_t config_image_view{ .handle_image = image };
    auto image_view = renderer.context.create_image_view(config_image_view);
    auto sampler = renderer.context.create_sampler({});
    auto swapchain_descriptor_set = renderer.allocate_descriptor_set(renderer::resource_policy_t::e_sparse, { .handle_descriptor_set_layout = swapchain_descriptor_set_layout });
    renderer.update_descriptor_set(swapchain_descriptor_set).push_image_write(0, renderer::image_descriptor_info_t{ .handle_sampler = sampler, .handle_image_view = image_view, .vk_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }).commit();

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

    float current_val = 0;

    while (!window.should_close()) {
        core::window_t::poll_events();

        renderer.begin(false);   
        
        auto swapchain_image_resource = rendergraph::resource_builder_t{"swapchain image"}.is_image(renderer.current_swapchain_image(), renderer.current_swapchain_image_view());
        auto offscreen_image_resource = rendergraph::resource_builder_t{"offscreen image"}.is_image(image, image_view, sampler);
        auto buffer_resource = rendergraph::resource_builder_t{"uniform buffer"}.is_buffer(buffer);

        rendergraph::rendergraph_t rendergraph{};
        rendergraph.add_task(rendergraph::task_builder_t{"update uniform", rendergraph::task_type_t::e_cpu}
                                .add_resource_refrenced_and_its_usage(buffer_resource, rendergraph::resource_usage_t::e_writing)
                                .with_callback([&](renderer::base_renderer_t& renderer) {
                                    current_val += .0001;
                                    if (current_val > 1.0) current_val = 0;
                                    *reinterpret_cast<float *>(renderer.context.map_buffer(renderer.buffer(buffer))) = current_val;
                                }))
                   .add_task(rendergraph::task_builder_t{"offscreen task", rendergraph::task_type_t::e_graphics}
                                .add_resource_refrenced_and_its_usage(offscreen_image_resource, rendergraph::resource_usage_t::e_writing)
                                .add_resource_refrenced_and_its_usage(buffer_resource, rendergraph::resource_usage_t::e_reading)
                                .with_callback([&](renderer::base_renderer_t& renderer) {
                                    auto commandbuffer = renderer.current_commandbuffer();
                                    gfx::rendering_attachment_t rendering_attachment{};
                                    rendering_attachment.clear_value = {0, 0, 0, 0};
                                    rendering_attachment.handle_image_view = image_view;
                                    rendering_attachment.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                                    rendering_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
                                    rendering_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
                                    renderer.context.cmd_begin_rendering(commandbuffer, {rendering_attachment}, std::nullopt, VkRect2D{VkOffset2D{}, {640, 420}});
                                    renderer.context.cmd_bind_pipeliine(commandbuffer, pipeline);
                                    renderer.context.cmd_bind_descriptor_sets(commandbuffer, pipeline, 0, { renderer.descriptor_set(descriptor_set) });
                                    renderer.context.cmd_set_viewport_and_scissor(commandbuffer, swapchain_viewport, swapchain_scissor);
                                    renderer.context.cmd_draw(commandbuffer, 3, 1, 0, 0);
                                    renderer.context.cmd_end_rendering(commandbuffer);
                                }))
                   .add_task(rendergraph::task_builder_t{"swapchain task", rendergraph::task_type_t::e_graphics}
                                .add_resource_refrenced_and_its_usage(swapchain_image_resource, rendergraph::resource_usage_t::e_writing)
                                .add_resource_refrenced_and_its_usage(offscreen_image_resource, rendergraph::resource_usage_t::e_reading)
                                .with_callback([&](renderer::base_renderer_t& renderer) {
                                    auto commandbuffer = renderer.current_commandbuffer();
                                    auto swapchain_rendering_attachment = renderer.swapchain_rendering_attachment({0, 0, 0, 0}, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
                                    renderer.context.cmd_begin_rendering(commandbuffer, {swapchain_rendering_attachment}, std::nullopt, VkRect2D{VkOffset2D{}, {640, 420}});
                                    renderer.context.cmd_bind_pipeliine(commandbuffer, swapchain_pipeline);
                                    renderer.context.cmd_bind_descriptor_sets(commandbuffer, swapchain_pipeline, 0, { renderer.descriptor_set(swapchain_descriptor_set) });
                                    renderer.context.cmd_set_viewport_and_scissor(commandbuffer, swapchain_viewport, swapchain_scissor);
                                    renderer.context.cmd_draw(commandbuffer, 6, 1, 0, 0);
                                    renderer.context.cmd_end_rendering(commandbuffer);
                                }));
        rendergraph.run(renderer);
        renderer.end();
    }
    
    return 0;
}
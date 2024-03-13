#include "test.hpp"

#define horizon_profile_enable
#include "core/core.hpp"
#include "core/window.hpp"
#include "gfx/context.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

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

void test_compute() {
    gfx::context_t context{ true };

    auto shader = context.create_shader_module(gfx::shader_module_config_t{ .code = compute_shader_code, .name = "compute", .type = gfx::shader_type_t::e_compute});

    gfx::descriptor_set_layout_config_t dsl_config{};
    dsl_config.add_layout_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    auto dsl = context.create_descriptor_set_layout(dsl_config);

    gfx::pipeline_config_t pipeline_config{};
    pipeline_config.shaders = { shader };
    pipeline_config.descriptor_set_layouts = { dsl };
    pipeline_config.add_push_constant(sizeof(int), 0, VK_SHADER_STAGE_COMPUTE_BIT);
    auto pipeline = context.create_compute_pipeline(pipeline_config);

    auto ds = context.allocate_descriptor_set(dsl);

    auto buffer = context.create_buffer(gfx::buffer_config_t{ .size = sizeof(int), .vk_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, .vma_allocation_create_flags = VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT });

    context.update_descriptor_set(ds)
           .push_write(0, gfx::buffer_descriptor_info_t{ .buffer = buffer, .vk_offset = 0, .vk_range = VK_WHOLE_SIZE })
           .commit();

    gfx::command_list_t command_list{};
    command_list.bind_pipeline(pipeline);
    command_list.bind_descriptor_sets<1>(pipeline, 0, { ds });
    command_list.push_constant(pipeline, 0, sizeof(int), VK_SHADER_STAGE_COMPUTE_BIT, 5);
    command_list.dispatch(1, 1, 1);

    auto command_pool = context.create_command_pool({});

    int *i = reinterpret_cast<int *>(context.map_buffer(buffer));
    *i = 0;
    context.exec_command_list_immediate(command_pool, command_list);
    horizon_info("{}", *i);
}

void new_test() {
    core::window_t window{ "test", 600, 400 };
    gfx::context_t context{ true };
    auto swapchain = context.create_swapchain(window);

    gfx::pipeline_config_t pipeline_config{};
    pipeline_config.add_shader(context.create_shader_module(gfx::shader_module_config_t{ .code = test_vertex, .name = "test-vert", .type = gfx::shader_type_t::e_vertex }));
    pipeline_config.add_shader(context.create_shader_module(gfx::shader_module_config_t{ .code = test_fragment, .name = "test-frag", .type = gfx::shader_type_t::e_fragment }));
    pipeline_config.add_color_attachment(VK_FORMAT_B8G8R8A8_SRGB, gfx::default_color_blend_attachment());
    auto pipeline = context.create_graphics_pipeline(pipeline_config);

    auto in_flight_fence = context.create_fence({});
    auto image_available_semaphore = context.create_semaphore({});
    auto render_finished_semaphore = context.create_semaphore({});

    auto command_pool = context.create_command_pool({});
    auto commandbuffers = context.allocate_commandbuffers<1>(command_pool);  // 1 frame in flight

    VkViewport swapchain_viewport{};
    swapchain_viewport.x = 0;
    swapchain_viewport.y = 0;
    swapchain_viewport.width = 600;
    swapchain_viewport.height = 400;
    swapchain_viewport.minDepth = 0;
    swapchain_viewport.maxDepth = 1;
    VkRect2D swapchain_scissor{};
    swapchain_scissor.offset = {0, 0};
    swapchain_scissor.extent = {600, 400};

    while (!window.should_close()) {
        core::window_t::poll_events();

        context.wait_fence<1>({in_flight_fence});

        auto next_image = context.acquire_swapchain_next_image_index(swapchain, image_available_semaphore, gfx::null_handle);
        if (!next_image) {
            horizon_error("Failed to get next image");
            exit(EXIT_FAILURE);
        }
        context.reset_fence<1>({in_flight_fence});

        context.begin_commandbuffer(commandbuffers[0]);

        gfx::command_begin_rendering_t begin_rendering{};
        begin_rendering.render_area = VkRect2D{VkOffset2D{}, { 600, 400 }};
        begin_rendering.layer_count = 1;
        begin_rendering.color_attachments_count = 1;
        begin_rendering.p_color_attachments[0].clear_value = {0, 0, 0, 0};
        begin_rendering.p_color_attachments[0].image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        begin_rendering.p_color_attachments[0].load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        begin_rendering.p_color_attachments[0].store_op = VK_ATTACHMENT_STORE_OP_STORE;
        begin_rendering.use_depth = false;

        gfx::command_list_t command_list{};

        command_list.image_memory_barrier(context.swapchain_images(swapchain)[*next_image], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        begin_rendering.p_color_attachments[0].image_view_handle = context.swapchain_image_views(swapchain)[*next_image];
        command_list.begin_rendering(begin_rendering);
        command_list.bind_pipeline(pipeline);
        command_list.set_viewport_and_scissor(swapchain_viewport, swapchain_scissor);
        command_list.draw(6, 1, 0, 0);
        command_list.end_rendering();
        command_list.image_memory_barrier(context.swapchain_images(swapchain)[*next_image], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

        context.exec_commands(commandbuffers[0], command_list);

        context.end_commandbuffer(commandbuffers[0]);

        context.submit_commandbuffer(commandbuffers[0], {image_available_semaphore}, {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}, {render_finished_semaphore}, in_flight_fence);

        context.present_swapchain<1>({swapchain}, {*next_image}, {render_finished_semaphore});

    }
}

int main() {
    test_compute();

    new_test();
    
    return 0;
}
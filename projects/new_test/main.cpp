#include "test.hpp"

#define horizon_profile_enable
#include "core/core.hpp"
#include "core/window.hpp"
#include "gfx/context.hpp"

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

layout(set = 0, binding = 0) uniform color_t {
    vec3 color;
};

void main() {
    outColor = vec4(color, 1.0);
}
)";

handle(buffer_handle_t);

enum class resource_usage_t {
    e_update_each_frame,
    e_update_once,
};

struct buffer_t {
    std::vector<gfx::buffer_handle_t> buffers;
    std::vector<gfx::descriptor_set_handle_t> descriptor_sets;
    resource_usage_t usage;      
};

struct buffer_descriptor_info_t {
    buffer_handle_t buffer;
    VkDeviceSize    vk_offset;
    VkDeviceSize    vk_range; 
};

template <size_t MAX_FRAMES_IN_FLIGHT>
struct renderer_t {
    renderer_t() : context( true ) {
        swapchain = context.create_swapchain(window);

        gfx::command_pool_config_t command_pool_config{};
        command_pool = context.create_command_pool(command_pool_config);

        commandbuffers = context.allocate_commandbuffers<MAX_FRAMES_IN_FLIGHT>(command_pool);

        gfx::fence_config_t fence_config{};
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            in_flight_fences[i] = context.create_fence(fence_config);
        }
        gfx::semaphore_config_t semaphore_config{};
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            image_available_semaphores[i] = context.create_semaphore(semaphore_config);
            rendering_finished_semaphores[i] = context.create_semaphore(semaphore_config);
        }
    }

    void start_frame() {
        context.wait_fence<1>({in_flight_fences[current_frame]});
        auto next_image = context.acquire_swapchain_next_image_index(swapchain, image_available_semaphores[current_frame], gfx::null_handle);
        if (!next_image) {
            horizon_error("Failed to get next image");
            std::terminate();
        }   
        this->next_image = *next_image;
        context.reset_fence<1>({in_flight_fences[current_frame]});
        context.begin_commandbuffer(commandbuffers[current_frame]);
    }

    void exec_command_list(const gfx::command_list_t& commandd_list) {
        context.exec_commands(commandbuffers[current_frame], commandd_list);
    }

    void end_frame() {
        context.end_commandbuffer(commandbuffers[current_frame]);
        context.submit_commandbuffer(commandbuffers[current_frame], {image_available_semaphores[current_frame]}, {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}, {rendering_finished_semaphores[current_frame]}, in_flight_fences[current_frame]);
        context.present_swapchain<1>({swapchain}, {next_image}, {rendering_finished_semaphores[current_frame]});
        current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    buffer_handle_t create_buffer(resource_usage_t usage, gfx::buffer_config_t config) {
        buffer_t buffer{ .usage = usage };
        if (usage == resource_usage_t::e_update_once) {
            buffer.buffers.push_back(context.create_buffer(config));
        } else {
            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                buffer.buffers.push_back(context.create_buffer(config));
            }
        }

        buffer_handle_t handle = _buffers.size();
        while (_buffers.contains(handle)) {
            handle++;
        }

        _buffers.insert({handle, buffer});

        return handle;
    }

    void allocate_buffer_descriptor_set(buffer_handle_t handle, gfx::descriptor_set_layout_handle_t descriptor_set_layout_handle) {
        assert(_buffers.contains(handle));
        buffer_t& buffer = _buffers[handle];
        for (size_t i = 0; i < buffer.buffers.size(); i++) {
            gfx::descriptor_set_handle_t descriptor_set_handle = context.allocate_descriptor_set(descriptor_set_layout_handle);
            buffer.descriptor_sets.push_back(descriptor_set_handle);
        }
    }

    gfx::buffer_handle_t buffer(buffer_handle_t handle) {
        assert(_buffers.contains(handle));
        buffer_t& buffer = _buffers[handle];
        if (buffer.usage == resource_usage_t::e_update_once) {
            return buffer.buffers[0];
        } else {
            return buffer.buffers[current_frame];
        }
    }

    gfx::descriptor_set_handle_t buffer_descriptor_set(buffer_handle_t handle) {
        assert(_buffers.contains(handle));
        buffer_t& buffer = _buffers[handle];
        if (buffer.usage == resource_usage_t::e_update_once) {
            return buffer.descriptor_sets[0];
        } else {
            return buffer.descriptor_sets[current_frame];
        }
    }

    void update_buffer_descriptor_set(buffer_handle_t handle, uint32_t binding, buffer_descriptor_info_t info, uint32_t count = 1) {
        assert(_buffers.contains(handle));
        buffer_t& buffer = _buffers[handle];
        for (size_t i = 0; i < buffer.buffers.size(); i++) {
            gfx::buffer_descriptor_info_t gfx_buffer_descriptor_info{};
            gfx_buffer_descriptor_info.buffer = buffer.buffers[i];
            gfx_buffer_descriptor_info.vk_offset = info.vk_offset;
            gfx_buffer_descriptor_info.vk_range = info.vk_range;
            context.update_descriptor_set(buffer.descriptor_sets[i]).push_write(binding, gfx_buffer_descriptor_info, count).commit();
        }
    }

    core::window_t window{ "test", 600, 400 };
    gfx::context_t context;
    gfx::swapchain_handle_t swapchain;
    uint32_t next_image;
private:

    gfx::command_pool_handle_t command_pool;
    std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> commandbuffers;

    std::array<gfx::fence_handle_t, MAX_FRAMES_IN_FLIGHT> in_flight_fences;
    std::array<gfx::semaphore_handle_t, MAX_FRAMES_IN_FLIGHT> image_available_semaphores;
    std::array<gfx::semaphore_handle_t, MAX_FRAMES_IN_FLIGHT> rendering_finished_semaphores;

    uint32_t current_frame = 0;
    

    std::map<buffer_handle_t, buffer_t> _buffers;
};

int main() {
    renderer_t<2> renderer;

    gfx::descriptor_set_layout_config_t descriptor_set_layout_config{};
    descriptor_set_layout_config.add_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    gfx::descriptor_set_layout_handle_t descriptor_set_layout = renderer.context.create_descriptor_set_layout(descriptor_set_layout_config);

    gfx::pipeline_config_t pipeline_config{};
    pipeline_config.add_shader(renderer.context.create_shader_module(gfx::shader_module_config_t{.code = test_vertex, .name = "test-vert", .type = gfx::shader_type_t::e_vertex}));
    pipeline_config.add_shader(renderer.context.create_shader_module(gfx::shader_module_config_t{.code = test_fragment, .name = "test-frag", .type = gfx::shader_type_t::e_fragment}));
    pipeline_config.add_color_attachment(VK_FORMAT_B8G8R8A8_SRGB, gfx::default_color_blend_attachment());
    pipeline_config.add_descriptor_set_layout(descriptor_set_layout);
    gfx::pipeline_handle_t pipeline = renderer.context.create_graphics_pipeline(pipeline_config);

    gfx::buffer_config_t buffer_config{};
    buffer_config.size = sizeof(float) * 3;
    buffer_config.vk_usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    buffer_config.vma_allocation_create_flags = VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    buffer_handle_t buffer = renderer.create_buffer(resource_usage_t::e_update_each_frame, buffer_config);
    renderer.allocate_buffer_descriptor_set(buffer, descriptor_set_layout);

    buffer_descriptor_info_t info{};
    info.buffer = buffer;
    info.vk_offset = 0;
    info.vk_range = VK_WHOLE_SIZE;
    renderer.update_buffer_descriptor_set(buffer, 0, info);

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


    while (!renderer.window.should_close()) {
        core::window_t::poll_events();
        renderer.start_frame();

        gfx::command_begin_rendering_t begin_rendering{};
        begin_rendering.render_area = VkRect2D{VkOffset2D{}, {600, 400}};
        begin_rendering.layer_count = 1;
        begin_rendering.color_attachments_count = 1;
        begin_rendering.use_depth = false;
        begin_rendering.p_color_attachments[0].clear_value = {0, 0, 0, 0};
        begin_rendering.p_color_attachments[0].image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        begin_rendering.p_color_attachments[0].load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        begin_rendering.p_color_attachments[0].store_op = VK_ATTACHMENT_STORE_OP_STORE;
        begin_rendering.p_color_attachments[0].image_view_handle = renderer.context.swapchain_image_views(renderer.swapchain)[renderer.next_image];

        static int i = 0;
        if (i == 0) {
            float *color = (float *)renderer.context.map_buffer(renderer.buffer(buffer));
            color[0] = 1;
            color[1] = 1;
            color[2] = 1;
        } else {
            float *color = (float *)renderer.context.map_buffer(renderer.buffer(buffer));
            color[0] = 1;
            color[1] = 0;
            color[2] = 0;
        }
        i = (i + 1) % 2;

        gfx::command_list_t command_list{};
        command_list.image_memory_barrier(renderer.context.swapchain_images(renderer.swapchain)[renderer.next_image], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        command_list.begin_rendering(begin_rendering);
        command_list.bind_pipeline(pipeline);
        command_list.set_viewport_and_scissor(swapchain_viewport, swapchain_scissor);
        command_list.bind_descriptor_sets<1>(pipeline, 0, {renderer.buffer_descriptor_set(buffer)});
        command_list.draw(6, 1, 0, 0);
        command_list.end_rendering();
        command_list.image_memory_barrier(renderer.context.swapchain_images(renderer.swapchain)[renderer.next_image], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

        renderer.exec_command_list(command_list);

        renderer.end_frame();
    }
    

    // core::window_t window{ "test", 600, 400 };
    // gfx::context_t context{ true };
    // auto swapchain = context.create_swapchain(window);

    // gfx::pipeline_config_t pipeline_config{};
    // pipeline_config.add_shader(context.create_shader_module(gfx::shader_module_config_t{ .code = test_vertex, .name = "test-vert", .type = gfx::shader_type_t::e_vertex }));
    // pipeline_config.add_shader(context.create_shader_module(gfx::shader_module_config_t{ .code = test_fragment, .name = "test-frag", .type = gfx::shader_type_t::e_fragment }));
    // pipeline_config.add_color_attachment(VK_FORMAT_B8G8R8A8_SRGB, gfx::default_color_blend_attachment());
    // auto pipeline = context.create_graphics_pipeline(pipeline_config);

    // auto in_flight_fence = context.create_fence({});
    // auto image_available_semaphore = context.create_semaphore({});
    // auto render_finished_semaphore = context.create_semaphore({});

    // auto command_pool = context.create_command_pool({});
    // auto commandbuffers = context.allocate_commandbuffers<1>(command_pool);  // 1 frame in flight

    // VkViewport swapchain_viewport{};
    // swapchain_viewport.x = 0;
    // swapchain_viewport.y = 0;
    // swapchain_viewport.width = 600;
    // swapchain_viewport.height = 400;
    // swapchain_viewport.minDepth = 0;
    // swapchain_viewport.maxDepth = 1;
    // VkRect2D swapchain_scissor{};
    // swapchain_scissor.offset = {0, 0};
    // swapchain_scissor.extent = {600, 400};

    // while (!window.should_close()) {
    //     core::window_t::poll_events();

    //     context.wait_fence<1>({in_flight_fence});

    //     auto next_image = context.acquire_swapchain_next_image_index(swapchain, image_available_semaphore, gfx::null_handle);
    //     if (!next_image) {
    //         horizon_error("Failed to get next image");
    //         std::terminate();
    //     }
    //     context.reset_fence<1>({in_flight_fence});

    //     context.begin_commandbuffer(commandbuffers[0]);

    //     gfx::command_begin_rendering_t begin_rendering{};
    //     begin_rendering.render_area = VkRect2D{VkOffset2D{}, { 600, 400 }};
    //     begin_rendering.layer_count = 1;
    //     begin_rendering.color_attachments_count = 1;
    //     begin_rendering.p_color_attachments[0].clear_value = {0, 0, 0, 0};
    //     begin_rendering.p_color_attachments[0].image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    //     begin_rendering.p_color_attachments[0].load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
    //     begin_rendering.p_color_attachments[0].store_op = VK_ATTACHMENT_STORE_OP_STORE;
    //     begin_rendering.use_depth = false;

    //     gfx::command_list_t command_list{};

    //     command_list.image_memory_barrier(context.swapchain_images(swapchain)[*next_image], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    //     begin_rendering.p_color_attachments[0].image_view_handle = context.swapchain_image_views(swapchain)[*next_image];
    //     command_list.begin_rendering(begin_rendering);
    //     command_list.bind_pipeline(pipeline);
    //     command_list.set_viewport_and_scissor(swapchain_viewport, swapchain_scissor);
    //     command_list.draw(6, 1, 0, 0);
    //     command_list.end_rendering();
    //     command_list.image_memory_barrier(context.swapchain_images(swapchain)[*next_image], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    //     context.exec_commands(commandbuffers[0], command_list);

    //     context.end_commandbuffer(commandbuffers[0]);

    //     context.submit_commandbuffer(commandbuffers[0], {image_available_semaphore}, {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}, {render_finished_semaphore}, in_flight_fence);

    //     context.present_swapchain<1>({swapchain}, {*next_image}, {render_finished_semaphore});

    // }
    
    return 0;
}
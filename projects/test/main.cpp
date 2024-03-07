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

int main() {
    gfx::context_t context{ true };

    auto shader = context.create_shader_module(compute_shader_code, gfx::shader_type_t::e_compute, "compute test");

    gfx::descriptor_set_layout_config_t dsl_config{};
    dsl_config.add_layout_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    auto dsl = context.create_descriptor_set_layout(dsl_config);

    gfx::pipeline_config_t pipeline_config{};
    pipeline_config.shaders = { shader };
    pipeline_config.descriptor_set_layouts = { dsl };
    pipeline_config.add_push_constant(sizeof(int), 0, VK_SHADER_STAGE_COMPUTE_BIT);
    auto pipeline = context.create_compute_pipeline(pipeline_config);

    auto ds = context.create_descriptor_set(dsl);

    auto buffer = context.create_buffer(gfx::buffer_config_t{ .size = sizeof(int), .usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, .vma_allocation_create_flags = VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT });

    context.update_descriptor_set(ds)
           .push_write(0, gfx::buffer_descriptor_info_t{ .buffer = buffer, .offset = 0, .range = VK_WHOLE_SIZE })
           .commit();

    gfx::command_list_t command_list{};
    command_list.bind_pipeline(pipeline);
    command_list.bind_descriptor_sets<1>(pipeline, 0, { ds });
    command_list.push_constant(pipeline, 0, sizeof(int), VK_SHADER_STAGE_COMPUTE_BIT, 5);
    command_list.dispatch(1, 1, 1);

    int *i = reinterpret_cast<int *>(context.map_buffer(buffer));
    *i = 0;
    context.exec_command_list_immediate(command_list);
    horizon_info("{}", *i);

    for (auto& [name, time] : core::get_frame_function_times()) {
        horizon_info("{}: {}", name, time);
    }

    gfx::image_config_t image_config{};
    image_config.width = 64;
    image_config.height = 64;
    image_config.depth = 1;
    image_config.type = VK_IMAGE_TYPE_2D;
    image_config.format = VK_FORMAT_R8G8B8A8_UNORM;
    image_config.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    image_config.vma_allocation_create_flags = {};
    auto image = context.create_image(image_config);

    context.destroy_image(image);

    gfx::pipeline_config_t gpc{};
    gpc.add_shader(context.create_shader_module(test_vertex, gfx::shader_type_t::e_vertex, "test"));
    gpc.add_shader(context.create_shader_module(test_fragment, gfx::shader_type_t::e_fragment, "test"));
    gpc.add_color_attachment(VK_FORMAT_R8G8B8A8_UNORM, gfx::default_color_blend_attachment());
    auto gp = context.create_graphics_pipeline(gpc);

    return 0;
}
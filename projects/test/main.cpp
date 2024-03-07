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

    return 0;
}
#include "core/core.hpp"
#include "core/window.hpp"
#include "core/model.hpp"

#include "gfx/context.hpp"
#include "gfx/base_renderer.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

struct gpu_mesh_t {
    gfx::handle_buffer_t vertex_buffer;
    gfx::handle_buffer_t index_buffer;
    uint32_t vertex_count;
    uint32_t index_count;

    gfx::handle_buffer_t scratch_buffer;
};

struct triangle_t {
    glm::vec3 p1, p2, p3;
};

int main() {
    core::window_t window{ "test", 640, 420 };
    renderer::base_renderer_t renderer{ window };
    gfx::context_t& context = renderer.context;

    gfx::config_descriptor_set_layout_t config_descriptor_set_layout{};
    config_descriptor_set_layout.add_layout_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    auto descriptor_set_layout = context.create_descriptor_set_layout(config_descriptor_set_layout);

    gfx::config_pipeline_layout_t config_pipeline_layout{};
    config_pipeline_layout.add_descriptor_set_layout(descriptor_set_layout);
    auto pipeline_layout = context.create_pipeline_layout(config_pipeline_layout);

    gfx::config_pipeline_t config_pipeline{};
    config_pipeline.add_shader(context.create_shader(gfx::config_shader_t{ .code = core::read_file("../../assets/shaders/test_compute/test.comp").data(), .name = "compute_shader", .type = gfx::shader_type_t::e_compute}));
    config_pipeline.handle_pipeline_layout = pipeline_layout;
    auto pipeline = context.create_compute_pipeline(config_pipeline);

    gfx::config_buffer_t config_buffer{};
    config_buffer.vk_size = 8 * sizeof(uint);
    config_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    config_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    auto buffer = context.create_buffer(config_buffer);

    auto descriptor_set = context.allocate_descriptor_set({.handle_descriptor_set_layout = descriptor_set_layout});
    context.update_descriptor_set(descriptor_set)
        .push_buffer_write(0, gfx::buffer_descriptor_info_t{.handle_buffer = buffer})
        .commit();

    for (int i = 0; i < 8; i++) {
        horizon_info("{}", reinterpret_cast<int *>(context.map_buffer(buffer))[i]);
    }
    
    for (int i = 0; i < 8; i++) {
        reinterpret_cast<int *>(context.map_buffer(buffer))[i] = 0;
    }

    for (int i = 0; i < 8; i++) {
        horizon_info("{}", reinterpret_cast<int *>(context.map_buffer(buffer))[i]);
    }

    auto fence = context.create_fence({});
    auto commandbuffer = context.allocate_commandbuffer({.handle_command_pool = renderer.command_pool});
    context.begin_commandbuffer(commandbuffer, true);
    context.cmd_bind_pipeline(commandbuffer, pipeline);
    context.cmd_bind_descriptor_sets(commandbuffer, pipeline, 0, { descriptor_set });
    context.cmd_dispatch(commandbuffer, 1, 1, 1);
    context.end_commandbuffer(commandbuffer);
    context.reset_fence(fence);
    context.submit_commandbuffer(commandbuffer, {}, {}, {}, fence);
    context.wait_fence(fence);

    for (int i = 0; i < 8; i++) {
        horizon_info("{}", reinterpret_cast<int *>(context.map_buffer(buffer))[i]);
    }

    // auto fence = context.create_fence({});

    // std::vector<gpu_mesh_t> gpu_meshes;

    // auto model = core::load_model_from_path("../../assets/models/triangle/triangle.obj");
    // {
    //     for (auto mesh : model.meshes) {
    //         gpu_mesh_t gpu_mesh{};

    //         gpu_mesh.vertex_count = mesh.vertices.size();
    //         gpu_mesh.index_count = mesh.indices.size();

    //         gfx::config_buffer_t config_vertex_buffer{};
    //         config_vertex_buffer.vk_size = mesh.vertices.size() * sizeof(core::vertex_t);
    //         config_vertex_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    //         config_vertex_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    //         auto staging_vertex_buffer = context.create_buffer(config_vertex_buffer);
    //         config_vertex_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    //         config_vertex_buffer.vma_allocation_create_flags = {};
    //         gpu_mesh.vertex_buffer = context.create_buffer(config_vertex_buffer);
    //         std::memcpy(context.map_buffer(staging_vertex_buffer), mesh.vertices.data(), config_vertex_buffer.vk_size);

    //         gfx::config_buffer_t config_index_buffer{};
    //         config_index_buffer.vk_size = mesh.indices.size() * sizeof(uint32_t);
    //         config_index_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    //         config_index_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    //         auto staging_index_buffer = context.create_buffer(config_index_buffer);
    //         config_index_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    //         config_index_buffer.vma_allocation_create_flags = {};
    //         gpu_mesh.index_buffer = context.create_buffer(config_index_buffer);
    //         std::memcpy(context.map_buffer(staging_index_buffer), mesh.indices.data(), config_index_buffer.vk_size);

    //         auto commandbuffer = context.allocate_commandbuffer({.handle_command_pool = renderer.command_pool});
    //         context.begin_commandbuffer(commandbuffer, true);
    //         context.cmd_copy_buffer(commandbuffer, staging_vertex_buffer, gpu_mesh.vertex_buffer, {.vk_size = config_vertex_buffer.vk_size});
    //         context.cmd_copy_buffer(commandbuffer, staging_index_buffer, gpu_mesh.index_buffer, {.vk_size = config_index_buffer.vk_size});
    //         context.end_commandbuffer(commandbuffer);

    //         context.reset_fence(fence);
    //         context.submit_commandbuffer(commandbuffer, {}, {}, {}, fence);
    //         context.wait_fence(fence);

    //         context.destroy_buffer(staging_vertex_buffer);
    //         context.destroy_buffer(staging_index_buffer);

    //         gfx::config_buffer_t config_scratch_buffer{};
    //         config_scratch_buffer.vk_size = (mesh.indices.size() / 3) * sizeof(triangle_t);
    //         config_scratch_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    //         // config_scratch_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_;
    //         gpu_mesh.scratch_buffer = context.create_buffer(config_scratch_buffer);

    //         gpu_meshes.push_back(gpu_mesh);
    //     }
    // }

    // gfx::config_descriptor_set_layout_t config_descriptor_set_layout{};
    // config_descriptor_set_layout.add_layout_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
    //                             .add_layout_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
    //                             .add_layout_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    // auto descriptor_set_layout = context.create_descriptor_set_layout(config_descriptor_set_layout);

    // gfx::config_pipeline_layout_t config_pipeline_layout{};
    // config_pipeline_layout.add_descriptor_set_layout(descriptor_set_layout);
    // auto pipeline_layout = context.create_pipeline_layout(config_pipeline_layout);

    // gfx::config_pipeline_t config_pipeline{};
    // config_pipeline.add_shader(context.create_shader(gfx::config_shader_t{ .code = core::read_file("../../assets/shaders/test_compute/glsl.comp").data(), .name = "compute_shader", .type = gfx::shader_type_t::e_compute}));
    // config_pipeline.handle_pipeline_layout = pipeline_layout;
    // auto pipeline = context.create_compute_pipeline(config_pipeline);

    // auto fence = context.create_fence({});
    // auto commandbuffer = context.allocate_commandbuffer({.handle_command_pool = renderer.command_pool});
    // context.begin_commandbuffer(commandbuffer, true);
    // context.cmd_dispatch(commandbuffer, gpu_meshes[0].index_count / 3, 1, 1);
    // context.end_commandbuffer(commandbuffer);

    return 0;
}
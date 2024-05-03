#ifndef UTILITY_HPP
#define UTILITY_HPP

#include "core/model.hpp"

#include "gfx/context.hpp"
#include "gfx/base_renderer.hpp"
#include "gfx/helper.hpp"

inline std::pair<VkViewport, VkRect2D> fill_viewport_and_scissor_structs(uint32_t width, uint32_t height) {
    VkViewport viewport{};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = width;
    viewport.height = height;
    viewport.minDepth = 0;
    viewport.maxDepth = 1;
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = { width, height };
    return { viewport, scissor };
}

static gfx::handle_descriptor_set_layout_t material_descriptor_set_layout{};

void create_material_descriptor_set_layout(gfx::context_t& context) {
    gfx::config_descriptor_set_layout_t config_material{};
    config_material.add_layout_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    material_descriptor_set_layout = context.create_descriptor_set_layout(config_material);
}

void destroy_material_descriptor_set_layout(gfx::context_t& context) {
    context.destroy_descriptor_set_layout(material_descriptor_set_layout);
}

struct gpu_mesh_t {
    gfx::handle_buffer_t vertex_buffer;
    gfx::handle_buffer_t index_buffer;
    uint32_t vertex_count;
    uint32_t index_count;

    gfx::handle_image_t albedo;
    gfx::handle_descriptor_set_t material_descriptor_set;
};

std::vector<gpu_mesh_t> load_model_from_path(gfx::context_t& context, gfx::handle_command_pool_t command_pool, const std::filesystem::path& model_path) {
    std::vector<gpu_mesh_t> gpu_meshes;
    core::model_t model = core::load_model_from_path(model_path);
    gfx::handle_fence_t fence = context.create_fence({});
    for (auto mesh : model.meshes) {
        gpu_mesh_t gpu_mesh{};
        gpu_mesh.vertex_count = mesh.vertices.size();
        gpu_mesh.index_count = mesh.indices.size();

        gfx::config_buffer_t config_vertex_buffer{};
        config_vertex_buffer.vk_size = mesh.vertices.size() * sizeof(core::vertex_t);
        config_vertex_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        config_vertex_buffer.vma_allocation_create_flags = {};
        gpu_mesh.vertex_buffer = gfx::helper::create_and_push_buffer_staged(context, command_pool, config_vertex_buffer, mesh.vertices.data());

        gfx::config_buffer_t config_index_buffer{};
        config_index_buffer.vk_size = mesh.indices.size() * sizeof(uint32_t);
        config_index_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        config_index_buffer.vma_allocation_create_flags = {};
        gpu_mesh.index_buffer = gfx::helper::create_and_push_buffer_staged(context, command_pool, config_index_buffer, mesh.indices.data());

        
        gpu_mesh.material_descriptor_set = context.allocate_descriptor_set(gfx::config_descriptor_set_t{.handle_descriptor_set_layout = material_descriptor_set_layout});


        gpu_meshes.push_back(gpu_mesh);
    }
    return gpu_meshes;
}

#endif
#ifndef RUNTIME_HPP
#define RUNTIME_HPP

#include "gfx/context.hpp"
#include "gfx/bindless_manager.hpp"
#include "gfx/helper.hpp"

#include "core/model.hpp"

namespace runtime {

struct material_t {
    gfx::handle_sampled_image_slot_t albedo;
};

struct mesh_t {
    VkDeviceAddress vertices;
    VkDeviceAddress indices;
    uint32_t model_transform_id;
    uint32_t material_id;
};

} // namespace runtime

// runtime::mesh_t runtime_mesh_from_mesh(const core::mesh_t& mesh) {
//     runtime::mesh_t r_mesh{};
    
//     gfx::config_buffer_t config_vertex_buffer{};
//     config_vertex_buffer.vk_size = mesh.vertices.size() * sizeof(core::vertex_t);
//     config_vertex_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
//     config_vertex_buffer.vma_allocation_create_flags = {};
//     gfx::handle_buffer_t vertex_buffer = gfx::helper::create_and_push_buffer_staged(context, command_pool, config_vertex_buffer, mesh.vertices.data());

//     gfx::config_buffer_t config_index_buffer{};
//     config_index_buffer.vk_size = mesh.indices.size() * sizeof(uint32_t);
//     config_index_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
//     config_index_buffer.vma_allocation_create_flags = {};
//     gfx::handle_buffer_t index_buffer = gfx::helper::create_and_push_buffer_staged(context, command_pool, config_index_buffer, mesh.indices.data());

// }

#endif
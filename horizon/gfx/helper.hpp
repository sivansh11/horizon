#ifndef GFX_HELPER_HPP
#define GFX_HELPER_HPP

#include "context.hpp"

// #define IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

namespace renderer {

class base_renderer_t;

} // namespace renderer


namespace gfx {

namespace helper {

std::pair<VkViewport, VkRect2D> fill_viewport_and_scissor_structs(uint32_t width, uint32_t height);

handle_buffer_t create_staging_buffer(context_t& context, VkDeviceSize vk_device_size);
handle_buffer_t create_and_push_buffer_staged(context_t& context, handle_command_pool_t command_pool, config_buffer_t config_buffer, const void *src);
template <typename type_t>
handle_buffer_t create_and_push_staged_vector(context_t& context, handle_command_pool_t command_pool, VkBufferUsageFlags vk_buffer_usage_flags, VmaAllocationCreateFlags vma_allocation_create_flags, const std::vector<type_t>& data) {
    config_buffer_t config_buffer{};
    config_buffer.vk_size = data.size() * sizeof(type_t);
    config_buffer.vk_buffer_usage_flags = vk_buffer_usage_flags;
    config_buffer.vma_allocation_create_flags = vma_allocation_create_flags;
    return create_and_push_buffer_staged(context, command_pool, config_buffer, data.data());
}

handle_commandbuffer_t start_single_use_commandbuffer(context_t& context, handle_command_pool_t command_pool);
void end_single_use_commandbuffer(context_t& context, handle_commandbuffer_t commandbuffer);

VkImageAspectFlags image_aspect_from_format(VkFormat vk_format);
handle_image_t load_image_from_path(context_t& context, handle_command_pool_t handle_command_pool, const std::filesystem::path& path, VkFormat vk_format);

void cmd_generate_image_mip_maps(context_t& context, handle_commandbuffer_t handle_commandbuffer, handle_image_t handle, VkImageLayout vk_old_layout, VkImageLayout vk_new_layout, VkFilter vk_filter);
void cmd_transition_image_layout(context_t& context, handle_commandbuffer_t handle_commandbuffer, handle_image_t handle, VkImageLayout vk_old_image_layout, VkImageLayout vk_new_image_layout, uint32_t base_mip_level = 0, uint32_t level_count = vk_auto_mips);

std::pair<handle_image_t, handle_image_view_t> create_2D_image_and_image_view(context_t& context, uint32_t width, uint32_t height, VkFormat vk_format, VkImageUsageFlags vk_usage);

template <typename T>
void copy_mappable_buffer(context_t& context, handle_buffer_t handle, const std::vector<T>& data, uint32_t offset = 0) {
    internal::buffer_t& buffer = context.get_buffer(handle);

    horizon_assert((buffer.config.vma_allocation_create_flags & VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT) || (buffer.config.vma_allocation_create_flags & VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT), "buffer is not mappable");
    horizon_assert(buffer.config.vk_size >= data.size() * sizeof(data[0]), "buffer doesnt have enough space");

    std::memcpy(reinterpret_cast<char *>(context.map_buffer(handle)) + offset, data.data(), data.size() * sizeof(data[0]));
}

// TODO: take target image format
void imgui_init(core::window_t& window, renderer::base_renderer_t& renderer, VkFormat vk_color_format);
void imgui_shutdown();
void imgui_newframe();
void imgui_endframe(renderer::base_renderer_t& renderer, gfx::handle_commandbuffer_t commandbuffer);

// template <typename func_t>
// void render(context_t& context, handle_commandbuffer_t handle_commandbuffer, const std::vector<rendering_attachment_t>& color_rendering_attachments, const std::optional<rendering_attachment_t>& depth_rendering_attachment, const VkRect2D& vk_render_area, uint32_t vk_layer_count, func_t func) {
//     context.cmd_begin_rendering(handle_commandbuffer, color_rendering_attachments, depth_rendering_attachment, vk_render_area, vk_layer_count);
//     func();
//     context.cmd_end_rendering(handle_commandbuffer);
// }

} // namespace helper

} // namespace gfx

#endif
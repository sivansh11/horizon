#ifndef GFX_HELPER_HPP
#define GFX_HELPER_HPP

#include "context.hpp"

namespace gfx {

namespace helper {

VkImageAspectFlags image_aspect_from_format(VkFormat vk_format);
handle_buffer_t create_staging_buffer(context_t& context, VkDeviceSize vk_device_size);
handle_commandbuffer_t start_single_use_commandbuffer(context_t& context, handle_command_pool_t command_pool);
void end_single_use_commandbuffer(context_t& context, handle_commandbuffer_t commandbuffer);
handle_buffer_t create_and_push_buffer_staged(context_t& context, handle_command_pool_t command_pool, config_buffer_t config_buffer, const void *src);
void cmd_generate_image_mip_maps(context_t& context, handle_commandbuffer_t handle_commandbuffer, handle_image_t handle, VkImageLayout vk_old_layout, VkImageLayout vk_new_layout, VkFilter vk_filter);
void cmd_transition_image_layout(context_t& context, handle_commandbuffer_t handle_commandbuffer, handle_image_t handle, VkImageLayout vk_old_image_layout, VkImageLayout vk_new_image_layout, uint32_t base_mip_level = 0, uint32_t level_count = vk_auto_mips);
handle_image_t load_image_from_path(context_t& context, handle_command_pool_t handle_command_pool, const std::filesystem::path& path, VkFormat vk_format);

} // namespace helper

} // namespace gfx

#endif
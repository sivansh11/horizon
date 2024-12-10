#ifndef GFX_HELPER_HPP
#define GFX_HELPER_HPP

#include "context.hpp"

#include <filesystem>

namespace gfx {

namespace helper {

std::pair<VkViewport, VkRect2D> fill_viewport_and_scissor_structs(uint32_t width, uint32_t height);

handle_commandbuffer_t begin_single_use_commandbuffer(context_t& context, handle_command_pool_t command_pool);
void end_single_use_command_buffer(context_t& context, handle_commandbuffer_t handle);

VkImageAspectFlags image_aspect_from_format(VkFormat vk_format);

void cmd_generate_image_mip_maps(context_t& context, handle_commandbuffer_t handle_commandbuffer, handle_image_t handle_image, VkImageLayout vk_image_layout_old, VkImageLayout vk_image_layout_new, VkFilter vk_filter);
handle_image_t load_image_from_path_instant(context_t& context, handle_command_pool_t handle_command_pool, const std::filesystem::path& path, VkFormat vk_format);
// TODO: create a image loader or something
// handle_image_t load_image_from_path(context_t& context, const std::filesystem::path& path, VkFormat vk_format);

}

}

#endif

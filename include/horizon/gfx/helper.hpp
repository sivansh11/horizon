#ifndef GFX_HELPER_HPP
#define GFX_HELPER_HPP

#include "context.hpp"
#include "horizon/gfx/types.hpp"

#include <filesystem>

#define GLFW_INCLUDE_NONE
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

namespace gfx {

namespace helper {

std::pair<VkViewport, VkRect2D> fill_viewport_and_scissor_structs(uint32_t width, uint32_t height);

handle_commandbuffer_t begin_single_use_commandbuffer(context_t& context, handle_command_pool_t command_pool);
void end_single_use_command_buffer(context_t& context, handle_commandbuffer_t handle);

VkImageAspectFlags image_aspect_from_format(VkFormat vk_format);

void cmd_transition_image_layout(context_t& context, handle_commandbuffer_t handle_commandbuffer, handle_image_t handle, VkImageLayout vk_old_image_layout, VkImageLayout vk_new_image_layout, uint32_t base_mip_level = 0, uint32_t level_count = vk_auto_mips);
void cmd_generate_image_mip_maps(context_t& context, handle_commandbuffer_t handle_commandbuffer, handle_image_t handle_image, VkImageLayout vk_image_layout_old, VkImageLayout vk_image_layout_new, VkFilter vk_filter);
handle_image_t load_image_from_path_instant(context_t& context, handle_command_pool_t handle_command_pool, const std::filesystem::path& path, VkFormat vk_format);
// TODO: create a image loader or something
// handle_image_t load_image_from_path(context_t& context, const std::filesystem::path& path, VkFormat vk_format);

handle_buffer_t create_buffer_staged(context_t& context, handle_command_pool_t handle_command_pool, config_buffer_t config, void *data, size_t size);

// TODO: take target image format
void imgui_init(core::window_t& window, context_t& context, handle_swapchain_t handle_swapchain, VkFormat vk_color_format);
void imgui_shutdown();
void imgui_newframe();
void imgui_endframe(context_t& context, gfx::handle_commandbuffer_t commandbuffer);

gfx::handle_shader_t create_slang_shader(context_t& context, const std::filesystem::path& file_path, shader_type_t type);

}

}

#endif

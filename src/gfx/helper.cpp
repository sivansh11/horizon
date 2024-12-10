#include "horizon/core/core.hpp"
#include "horizon/core/logger.hpp"
#include "horizon/core/stb_image.h"

#include "horizon/gfx/helper.hpp"
#include "horizon/gfx/context.hpp"


#include <compare>
#include <set>

namespace gfx {

namespace helper {

std::pair<VkViewport, VkRect2D> fill_viewport_and_scissor_structs(uint32_t width, uint32_t height) {
    horizon_profile();
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


handle_commandbuffer_t begin_single_use_commandbuffer(context_t& context, handle_command_pool_t command_pool) {
    horizon_profile();
    handle_commandbuffer_t cbuf = context.allocate_commandbuffer({ .handle_command_pool = command_pool });
    context.begin_commandbuffer(cbuf);
    return cbuf;
}


void end_single_use_command_buffer(context_t& context, handle_commandbuffer_t handle) {
    horizon_profile();
    handle_fence_t fence = context.create_fence({});
    context.end_commandbuffer(handle);
    context.reset_fence(fence);
    context.submit_commandbuffer(handle, {}, {}, {}, fence);
    context.wait_fence(fence);
    context.destroy_fence(fence);
    context.free_commandbuffer(handle);
}


VkImageAspectFlags image_aspect_from_format(VkFormat vk_format) {
    horizon_profile();
    static std::set<VkFormat> depth_formats = {
        VK_FORMAT_D16_UNORM,
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
    };

    if (depth_formats.contains(vk_format)) 
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    return VK_IMAGE_ASPECT_COLOR_BIT;
}


void cmd_generate_image_mip_maps(context_t& context, handle_commandbuffer_t handle_commandbuffer, handle_image_t handle, VkImageLayout vk_old_layout, VkImageLayout vk_new_layout, VkFilter vk_filter) {
    horizon_profile();
    internal::image_t& image = context.get_image(handle);
    check(image.config.vk_mips != 1, "cannot generate mip maps for image!");            // maybe dont fail if cannot generate ?

    VkImageMemoryBarrier vk_image_memory_barrier{};
    vk_image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    vk_image_memory_barrier.srcAccessMask = 0;
    vk_image_memory_barrier.dstAccessMask = 0;
    vk_image_memory_barrier.oldLayout = {};
    vk_image_memory_barrier.newLayout = {};
    vk_image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vk_image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vk_image_memory_barrier.image = image;
    vk_image_memory_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vk_image_memory_barrier.subresourceRange.baseMipLevel = {};
    vk_image_memory_barrier.subresourceRange.levelCount = 1;
    vk_image_memory_barrier.subresourceRange.baseArrayLayer = 0;
    vk_image_memory_barrier.subresourceRange.layerCount = 1;

    uint32_t mip_width = image.config.vk_width;
    uint32_t mip_height = image.config.vk_height;
    uint32_t mip_depth = image.config.vk_depth;

    for (size_t i = 1; i < image.config.vk_mips; i++) {
        vk_image_memory_barrier.subresourceRange.baseMipLevel = i - 1;
        vk_image_memory_barrier.oldLayout = vk_old_layout;
        vk_image_memory_barrier.newLayout = vk_old_layout != VK_IMAGE_LAYOUT_GENERAL ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
        vk_image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vk_image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        context.cmd_pipeline_barrier(handle_commandbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, {}, {}, { vk_image_memory_barrier });

        VkImageBlit vk_image_blit{};
        vk_image_blit.srcSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = static_cast<uint32_t>(i - 1),
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
        vk_image_blit.srcOffsets[0] = {0, 0, 0};
        vk_image_blit.srcOffsets[1] = { static_cast<int32_t>(mip_width), static_cast<int32_t>(mip_height), static_cast<int32_t>(mip_depth) };
        vk_image_blit.dstSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = static_cast<uint32_t>(i),
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
        vk_image_blit.dstOffsets[0] = {0, 0, 0};
        vk_image_blit.dstOffsets[1] = { mip_width > 1 ? static_cast<int32_t>(mip_width / 2) : 1, mip_height > 1 ? static_cast<int32_t>(mip_height / 2) : 1, mip_depth > 1 ? static_cast<int32_t>(mip_depth / 2) : 1 };

        context.cmd_blit_image(handle_commandbuffer, handle, vk_old_layout != VK_IMAGE_LAYOUT_GENERAL ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL, handle, vk_new_layout != VK_IMAGE_LAYOUT_GENERAL ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL, { vk_image_blit }, vk_filter);

        vk_image_memory_barrier.oldLayout = vk_new_layout != VK_IMAGE_LAYOUT_GENERAL ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
        vk_image_memory_barrier.newLayout = vk_new_layout;
        vk_image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vk_image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        context.cmd_pipeline_barrier(handle_commandbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, {}, {}, { vk_image_memory_barrier });

        if (mip_width > 1) mip_width /= 2;
        if (mip_height > 1) mip_height /= 2;
        if (mip_depth > 1) mip_depth /= 2;
    }

    vk_image_memory_barrier.subresourceRange.baseMipLevel = image.config.vk_mips - 1;
    vk_image_memory_barrier.oldLayout = vk_old_layout;
    vk_image_memory_barrier.newLayout = vk_new_layout;
    vk_image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vk_image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    context.cmd_pipeline_barrier(handle_commandbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, {}, {}, { vk_image_memory_barrier });
}

handle_image_t load_image_from_path_instant(context_t& context, handle_command_pool_t handle_command_pool, const std::filesystem::path& path, VkFormat vk_format) {
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    stbi_uc *pixels = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
    check(pixels, "{}", stbi_failure_reason());

    VkDeviceSize vk_image_size = width * height * 4;
    horizon_trace("{} image loaded, width: {} height: {} device memory: {}", path.string(), width, height, vk_image_size);

    config_buffer_t config_staging_buffer{};

    // handle_buffer_t staging_buffer = 

    check(false, "Not implemented!");

    return {};
}

}

}

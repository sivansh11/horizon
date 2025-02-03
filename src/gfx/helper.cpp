#include "horizon/core/core.hpp"
#include "horizon/core/logger.hpp"
#include "horizon/core/stb_image.h"
#include "horizon/core/window.hpp"

#include "horizon/gfx/helper.hpp"
#include "horizon/gfx/context.hpp"

#include <compare>
#include <cstring>
#include <set>
#include <vulkan/vulkan_core.h>

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
    context.begin_commandbuffer(cbuf, true);
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


void cmd_transition_image_layout(context_t& context, handle_commandbuffer_t handle_commandbuffer, handle_image_t handle, VkImageLayout vk_old_image_layout, VkImageLayout vk_new_image_layout, uint32_t base_mip_level, uint32_t level_count) {
    internal::image_t& image = context.get_image(handle);

    VkImageMemoryBarrier vk_image_memory_barrier{};
    vk_image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    vk_image_memory_barrier.oldLayout = vk_old_image_layout;
    vk_image_memory_barrier.newLayout = vk_new_image_layout;
    vk_image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vk_image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vk_image_memory_barrier.image = image;
    vk_image_memory_barrier.subresourceRange.aspectMask = image_aspect_from_format(image.config.vk_format);
    vk_image_memory_barrier.subresourceRange.baseMipLevel = base_mip_level;
    if (level_count == vk_auto_mips) vk_image_memory_barrier.subresourceRange.levelCount = image.config.vk_mips - base_mip_level;
    else vk_image_memory_barrier.subresourceRange.levelCount = level_count;
    vk_image_memory_barrier.subresourceRange.baseArrayLayer = 0;
    vk_image_memory_barrier.subresourceRange.layerCount = 1;
    vk_image_memory_barrier.srcAccessMask = 0;
    vk_image_memory_barrier.dstAccessMask = 0;

    switch (vk_old_image_layout) {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            // Image layout is undefined (or does not matter)
            // Only valid as initial layout
            // No flags required, listed only for completeness
            vk_image_memory_barrier.srcAccessMask = 0;
            break;

        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            // Image is preinitialized
            // Only valid as initial layout for linear images, preserves memory contents
            // Make sure host writes have been finished
            vk_image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            // Image is a color attachment
            // Make sure any writes to the color buffer have been finished
            vk_image_memory_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            // Image is a depth/stencil attachment
            // Make sure any writes to the depth/stencil buffer have been finished
            vk_image_memory_barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            // Image is a transfer source
            // Make sure any reads from the image have been finished
            vk_image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            // Image is a transfer destination
            // Make sure any writes to the image have been finished
            vk_image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            // Image is read by a shader
            // Make sure any shader reads from the image have been finished
            vk_image_memory_barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;
        default:
            // Other source layouts aren't handled (yet)
            horizon_error("not handled transition");
            break;
    }

    switch (vk_new_image_layout) {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            // Image will be used as a transfer destination
            // Make sure any writes to the image have been finished
            vk_image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            // Image will be used as a transfer source
            // Make sure any reads from the image have been finished
            vk_image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            // Image will be used as a color attachment
            // Make sure any writes to the color buffer have been finished
            vk_image_memory_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            // Image layout will be used as a depth/stencil attachment
            // Make sure any writes to depth/stencil buffer have been finished
            vk_image_memory_barrier.dstAccessMask = vk_image_memory_barrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            // Image will be read in a shader (sampler, input attachment)
            // Make sure any writes to the image have been finished
            if (vk_image_memory_barrier.srcAccessMask == 0) {
                vk_image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            }
            vk_image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_GENERAL:
            vk_image_memory_barrier.srcAccessMask = 0;
            vk_image_memory_barrier.dstAccessMask = 0;
            break;

        default:
            // Other source layouts aren't handled (yet)
            horizon_error("not handled transition");
            break;
    };

    context.cmd_pipeline_barrier(handle_commandbuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, {}, {}, { vk_image_memory_barrier });
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
    config_staging_buffer.vk_size = vk_image_size;
    config_staging_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    config_staging_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    handle_buffer_t staging_buffer = context.create_buffer(config_staging_buffer);
    std::memcpy(context.map_buffer(staging_buffer), pixels, vk_image_size);

    stbi_image_free(pixels);

    config_image_t config_image{};
    config_image.vk_width = width;
    config_image.vk_height = height;
    config_image.vk_depth = 1;
    config_image.vk_type = VK_IMAGE_TYPE_2D;
    config_image.vk_format = vk_format;
    config_image.vk_usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    handle_image_t image = context.create_image(config_image);

    handle_commandbuffer_t cbuf = begin_single_use_commandbuffer(context, handle_command_pool);
    cmd_transition_image_layout(context, cbuf, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    context.cmd_copy_buffer_to_image(cbuf, staging_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageOffset = { 0, 0, 0 },
        .imageExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1u },
    });
    cmd_generate_image_mip_maps(context, cbuf, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_FILTER_LINEAR);
    end_single_use_command_buffer(context, cbuf);

    context.destroy_buffer(staging_buffer);

    return image;
}

handle_buffer_t create_buffer_staged(context_t& context, handle_command_pool_t handle_command_pool, config_buffer_t config, const void *data, size_t size) {
    horizon_assert(size <= config.vk_size, "copying more than allocated");

    config.vk_buffer_usage_flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    handle_buffer_t buffer = context.create_buffer(config);

    config_buffer_t config_staging_buffer{};
    config_staging_buffer.vk_size = config.vk_size;
    config_staging_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    config_staging_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    handle_buffer_t staging_buffer = context.create_buffer(config_staging_buffer);

    std::memcpy(context.map_buffer(staging_buffer), data, size);

    handle_commandbuffer_t cbuf = begin_single_use_commandbuffer(context, handle_command_pool);
    context.cmd_copy_buffer(cbuf, staging_buffer, buffer, {
        .vk_src_offset = 0,
        .vk_dst_offset = 0,
        .vk_size = size
    });
    end_single_use_command_buffer(context, cbuf);

  context.destroy_buffer(staging_buffer);

    return buffer;
}


static void checkVkResult(VkResult error) {
    if (error == 0) return;
    horizon_error("imgui error: {}", uint64_t(error));
    if (error < 0) abort();
}

void imgui_init(core::window_t& window, context_t& context, handle_swapchain_t handle_swapchain, VkFormat vk_color_format) {
    horizon_profile();

    IMGUI_CHECKVERSION();

    ImGui::CreateContext();

    ImGuiIO& IO = ImGui::GetIO();
    IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = context.instance();
    initInfo.PhysicalDevice = context.physical_device();
    initInfo.Device = context.device();
    initInfo.QueueFamily = context.graphics_queue().vk_index;
    initInfo.Queue = context.graphics_queue();

    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = context.descriptor_pool();

    initInfo.Allocator = VK_NULL_HANDLE;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = context.get_swapchain_images(handle_swapchain).size();
    initInfo.CheckVkResultFn = checkVkResult;
    initInfo.Subpass = 0;

    initInfo.UseDynamicRendering = true;
    std::vector<VkFormat> vk_color_formats{
        vk_color_format,
    };
    VkPipelineRenderingCreateInfo vk_pipeline_rendering_create{ .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    vk_pipeline_rendering_create.pNext                   = nullptr;
    vk_pipeline_rendering_create.colorAttachmentCount    = vk_color_formats.size();
    vk_pipeline_rendering_create.pColorAttachmentFormats = vk_color_formats.data();
    vk_pipeline_rendering_create.depthAttachmentFormat   = VK_FORMAT_UNDEFINED;
    vk_pipeline_rendering_create.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
    initInfo.PipelineRenderingCreateInfo = vk_pipeline_rendering_create;

    ImGui_ImplVulkan_LoadFunctions([](const char *function_name, void *vulkan_instance) {
        return vkGetInstanceProcAddr(*(reinterpret_cast<VkInstance *>(vulkan_instance)), function_name);
    }, &context.instance().instance);

    ImGui_ImplVulkan_Init(&initInfo);

    ImGui_ImplVulkan_CreateFontsTexture();
}

void imgui_shutdown() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void imgui_newframe() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void imgui_endframe(context_t& context, gfx::handle_commandbuffer_t commandbuffer) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), context.get_commandbuffer(commandbuffer));
}

gfx::handle_shader_t create_slang_shader(context_t& context, const std::filesystem::path& file_path, shader_type_t type) {
  config_shader_t cs{};
  cs.code_or_path = file_path.string();
  cs.is_code = false;
  cs.name = file_path.filename();
  cs.type = type;
  cs.language = shader_language_t::e_slang;
  return context.create_shader(cs);
}

}

}

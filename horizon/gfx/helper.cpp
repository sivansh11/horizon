#include "helper.hpp"

#include "base_renderer.hpp"

#include "core/window.hpp"

#include <stb_image/stb_image.hpp>

#include <set>

namespace gfx {

namespace helper {

std::pair<VkViewport, VkRect2D> fill_viewport_and_scissor_structs(uint32_t width, uint32_t height) {
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

handle_buffer_t create_staging_buffer(context_t& context, VkDeviceSize vk_device_size) {
    config_buffer_t config_buffer{};
    config_buffer.vk_size = vk_device_size;
    config_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    config_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    handle_buffer_t staging_buffer = context.create_buffer(config_buffer);
    return staging_buffer;
}

handle_buffer_t create_and_push_buffer_staged(context_t& context, handle_command_pool_t command_pool, config_buffer_t config_buffer, const void *src) {
    config_buffer_t config_staging_buffer{};
    config_staging_buffer.vk_size = config_buffer.vk_size;
    config_staging_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    config_staging_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    handle_buffer_t staging_buffer = context.create_buffer(config_staging_buffer);
    config_buffer.vk_buffer_usage_flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    handle_buffer_t buffer = context.create_buffer(config_buffer);

    std::memcpy(context.map_buffer(staging_buffer), src, config_buffer.vk_size);

    handle_commandbuffer_t commandbuffer = start_single_use_commandbuffer(context, command_pool);
    context.cmd_copy_buffer(commandbuffer, staging_buffer, buffer, { .vk_size = config_buffer.vk_size });
    end_single_use_commandbuffer(context, commandbuffer);

    context.destroy_buffer(staging_buffer);
    return buffer;
}

handle_commandbuffer_t start_single_use_commandbuffer(context_t& context, handle_command_pool_t command_pool) {
    handle_commandbuffer_t commandbuffer = context.allocate_commandbuffer({.handle_command_pool = command_pool});
    context.begin_commandbuffer(commandbuffer, true);
    return commandbuffer;
}

void end_single_use_commandbuffer(context_t& context, handle_commandbuffer_t commandbuffer) {
    handle_fence_t fence = context.create_fence({});
    context.end_commandbuffer(commandbuffer);
    context.reset_fence(fence);
    context.submit_commandbuffer(commandbuffer, {}, {}, {}, fence);
    context.wait_fence(fence);
    context.destroy_fence(fence);
    // context.free_commandbuffer(commandbuffer);
}

VkImageAspectFlags image_aspect_from_format(VkFormat vk_format) {
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

handle_image_t load_image_from_path(context_t& context, handle_command_pool_t handle_command_pool, const std::filesystem::path& path, VkFormat vk_format) {

    if (path.extension().string() == ".ppm") {
        auto get_words_from_line = [](const std::string& line) {
            std::vector<std::string> words;
            std::istringstream iss(line);
            for (std::string word; iss >> word;) {
                words.push_back(word);
            }
            return words;
        };

        std::string ppm_file = core::read_file(path);
        std::stringstream ss{ ppm_file };
        std::string line;
        uint32_t i = 0;

        uint32_t width, height;

        struct pixel_t {
            uint8_t r;
            uint8_t g;
            uint8_t b;
            uint8_t a;
        };
        std::vector<pixel_t> pixels;

        while (std::getline(ss, line, '\n')) {
            if (i == 0) {
                i++;
                continue;
            }

            if (i == 1) {
                auto words = get_words_from_line(line);
                width = std::stol(words[0]);
                height = std::stol(words[1]);
                i++;
                continue;
            }

            if (i == 2) {
                i++;
                continue;
            }

            auto words = get_words_from_line(line);
            pixel_t pixel = { static_cast<uint8_t>(std::stoi(words[0])), static_cast<uint8_t>(std::stoi(words[1])), static_cast<uint8_t>(std::stoi(words[2])), 255 };
            pixels.push_back(pixel);
            i++;
        }

        VkDeviceSize vk_image_size = width * height * 4;
        handle_buffer_t staging_buffer = create_staging_buffer(context, vk_image_size);

        std::memcpy(context.map_buffer(staging_buffer), pixels.data(), vk_image_size);

        config_image_t config_image{};
        config_image.vk_width = width;
        config_image.vk_height = height;
        config_image.vk_depth = 1;
        config_image.vk_type = VK_IMAGE_TYPE_2D;
        config_image.vk_format = vk_format;
        config_image.vk_usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        config_image.debug_name = path.string();
        handle_image_t image = context.create_image(config_image);

        handle_commandbuffer_t commandbuffer = start_single_use_commandbuffer(context, handle_command_pool);
        cmd_transition_image_layout(context, commandbuffer, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        context.cmd_copy_buffer_to_image(commandbuffer, staging_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VkBufferImageCopy{
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .imageOffset = {0, 0, 0},
            .imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1}
        });
        cmd_generate_image_mip_maps(context, commandbuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_FILTER_LINEAR);
        end_single_use_commandbuffer(context, commandbuffer);

        context.destroy_buffer(staging_buffer);

        return image;
    }

    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    stbi_uc *pixels = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
    check(pixels, "{}", stbi_failure_reason());

    VkDeviceSize vk_image_size = width * height * 4;
    handle_buffer_t staging_buffer = create_staging_buffer(context, vk_image_size);
    std::memcpy(context.map_buffer(staging_buffer), pixels, vk_image_size);

    stbi_image_free(pixels);

    // uint32_t                 vk_width;
    // uint32_t                 vk_height;
    // uint32_t                 vk_depth;
    // VkImageType              vk_type;
    // VkFormat                 vk_format;
    // VkImageUsageFlags        vk_usage;
    // VmaMemoryUsage           vma_memory_usage = VMA_MEMORY_USAGE_AUTO;
    // uint32_t                 vk_mips = vk_auto_calculate_mip_levels;
    // VkSampleCountFlagBits    vk_sample_count = VK_SAMPLE_COUNT_1_BIT;
    // uint32_t                 vk_array_layers = 1;
    // VkImageTiling            vk_tiling = VK_IMAGE_TILING_OPTIMAL;
    // VkImageLayout            vk_initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    // VmaAllocationCreateFlags vma_allocation_create_flags;

    config_image_t config_image{};
    config_image.vk_width = width;
    config_image.vk_height = height;
    config_image.vk_depth = 1;
    config_image.vk_type = VK_IMAGE_TYPE_2D;
    config_image.vk_format = vk_format;
    config_image.vk_usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    handle_image_t image = context.create_image(config_image);

    handle_commandbuffer_t commandbuffer = start_single_use_commandbuffer(context, handle_command_pool);
    cmd_transition_image_layout(context, commandbuffer, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    context.cmd_copy_buffer_to_image(commandbuffer, staging_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VkBufferImageCopy{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1}
    });
    cmd_generate_image_mip_maps(context, commandbuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_FILTER_LINEAR);
    end_single_use_commandbuffer(context, commandbuffer);

    context.destroy_buffer(staging_buffer);

    return image;
}

void cmd_generate_image_mip_maps(context_t& context, handle_commandbuffer_t handle_commandbuffer, handle_image_t handle, VkImageLayout vk_old_layout, VkImageLayout vk_new_layout, VkFilter vk_filter) {
    internal::image_t& image = context.get_image(handle);
    check(image.config.vk_mips != 1, "cannot generate mip maps for image");

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

std::pair<handle_image_t, handle_image_view_t> create_2D_image_and_image_view(context_t& context, uint32_t width, uint32_t height, VkFormat vk_format, VkImageUsageFlags vk_usage, std::string debug_name) {
    gfx::config_image_t config_target_image{};
    config_target_image.vk_width = width;
    config_target_image.vk_height = height;
    config_target_image.vk_depth = 1;
    config_target_image.vk_type = VK_IMAGE_TYPE_2D;
    config_target_image.vk_format = vk_format;
    config_target_image.vk_usage = vk_usage | VK_IMAGE_USAGE_SAMPLED_BIT;
    config_target_image.vk_mips = 1;
    config_target_image.debug_name = debug_name;
    handle_image_t target_image = context.create_image(config_target_image);
    gfx::handle_image_view_t target_image_view = context.create_image_view({ .handle_image = target_image, .debug_name = debug_name + "_view" });
    return { target_image, target_image_view };
}

static void checkVkResult(VkResult error) {
    if (error == 0) return;
    horizon_error("imgui error: {}", uint64_t(error));
    if (error < 0) abort();
}

void imgui_init(core::window_t& window, renderer::base_renderer_t& renderer, VkFormat vk_color_format) {
    horizon_profile();

    IMGUI_CHECKVERSION();

    ImGui::CreateContext();

    ImGuiIO& IO = ImGui::GetIO();
    IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = renderer.context.instance();
    initInfo.PhysicalDevice = renderer.context.physical_device();
    initInfo.Device = renderer.context.device();
    initInfo.QueueFamily = renderer.context.graphics_queue().vk_index;
    initInfo.Queue = renderer.context.graphics_queue();

    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = renderer.context.descriptor_pool();

    initInfo.Allocator = VK_NULL_HANDLE;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = renderer.context.get_swapchain_images(renderer.swapchain).size();
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
    }, &renderer.context.instance().instance);

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

void imgui_endframe(renderer::base_renderer_t& renderer, gfx::handle_commandbuffer_t commandbuffer) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), renderer.context.get_commandbuffer(commandbuffer));
}

} // namespace helper

} // namespace gfx

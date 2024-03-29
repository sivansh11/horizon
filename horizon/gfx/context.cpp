#include "context.hpp"

#include "core/window.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <shaderc/shaderc.hpp>
#include <shaderc/glslc/src/file_includer.h>
#include <shaderc/libshaderc_util/include/libshaderc_util/file_finder.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <iostream>
#include <set>

namespace utils {

static VkImageAspectFlags get_image_aspect(VkFormat vk_format) {
    horizon_profile();
    static std::set<VkFormat> vk_depth_formats = {
        VK_FORMAT_D16_UNORM,
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
    };

    if (vk_depth_formats.contains(vk_format)) 
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    return VK_IMAGE_ASPECT_COLOR_BIT;
}

} // namespace utils

namespace gfx {

config_descriptor_set_layout_t& config_descriptor_set_layout_t::add_layout_binding(uint32_t vk_binding, VkDescriptorType vk_descriptor_type, VkShaderStageFlags vk_shader_stages, uint32_t vk_count) {
    horizon_profile();
    VkDescriptorSetLayoutBinding vk_descriptor_set_layout_binding{};
    vk_descriptor_set_layout_binding.binding = vk_binding;
    vk_descriptor_set_layout_binding.descriptorType = vk_descriptor_type;
    vk_descriptor_set_layout_binding.stageFlags = vk_shader_stages;
    vk_descriptor_set_layout_binding.descriptorCount = vk_count;
    vk_descriptor_set_layout_bindings.push_back(vk_descriptor_set_layout_binding);
    return *this;
}

config_pipeline_layout_t& config_pipeline_layout_t::add_descriptor_set_layout(handle_descriptor_set_layout_t handle) {
    horizon_profile();
    handle_descriptor_set_layouts.push_back(handle);
    return *this;
}

config_pipeline_layout_t& config_pipeline_layout_t::add_push_constant(uint32_t vk_size, VkShaderStageFlagBits vk_shader_stages, uint32_t vk_offset) {
    horizon_profile();
    VkPushConstantRange vk_push_constant_range{};
    vk_push_constant_range.size = vk_size;
    vk_push_constant_range.offset = vk_offset;
    vk_push_constant_range.stageFlags = vk_shader_stages;
    vk_push_constant_ranges.push_back(vk_push_constant_range);
    return *this;
}

config_pipeline_t::config_pipeline_t() {
    horizon_profile();
    vk_pipeline_input_assembly_state = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, .primitiveRestartEnable = VK_FALSE };

    vk_pipeline_depth_stencil_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    vk_pipeline_depth_stencil_state_create_info.depthTestEnable = VK_TRUE;
    vk_pipeline_depth_stencil_state_create_info.depthWriteEnable = VK_TRUE;
    vk_pipeline_depth_stencil_state_create_info.depthCompareOp = VK_COMPARE_OP_LESS;
    vk_pipeline_depth_stencil_state_create_info.depthBoundsTestEnable = VK_FALSE;
    vk_pipeline_depth_stencil_state_create_info.minDepthBounds = 0.0f; // Optional
    vk_pipeline_depth_stencil_state_create_info.maxDepthBounds = 1.0f; // Optional
    vk_pipeline_depth_stencil_state_create_info.stencilTestEnable = VK_FALSE;
    vk_pipeline_depth_stencil_state_create_info.front = {}; // Optional
    vk_pipeline_depth_stencil_state_create_info.back = {}; // Optional
    
    vk_pipeline_rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    vk_pipeline_rasterization_state.depthClampEnable = VK_FALSE;
    vk_pipeline_rasterization_state.rasterizerDiscardEnable = VK_FALSE;
    vk_pipeline_rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
    vk_pipeline_rasterization_state.lineWidth = 1.0f;
    vk_pipeline_rasterization_state.cullMode = VK_CULL_MODE_NONE;
    vk_pipeline_rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE;
    vk_pipeline_rasterization_state.depthBiasEnable = VK_FALSE;
    vk_pipeline_rasterization_state.depthBiasConstantFactor = 0.0f; // Optional
    vk_pipeline_rasterization_state.depthBiasClamp = 0.0f; // Optional
    vk_pipeline_rasterization_state.depthBiasSlopeFactor = 0.0f; // Optional

    vk_pipeline_multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    vk_pipeline_multisample_state.sampleShadingEnable = VK_FALSE;
    vk_pipeline_multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    vk_pipeline_multisample_state.minSampleShading = 1.0f; // Optional
    vk_pipeline_multisample_state.pSampleMask = nullptr; // Optional
    vk_pipeline_multisample_state.alphaToCoverageEnable = VK_FALSE; // Optional
    vk_pipeline_multisample_state.alphaToOneEnable = VK_FALSE; // Optional

    vk_dynamic_states = { VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT };
}

config_pipeline_t& config_pipeline_t::add_shader(handle_shader_t handle) {
    horizon_profile();
    handle_shaders.push_back(handle);
    return *this;
}

config_pipeline_t& config_pipeline_t::add_color_attachment(VkFormat vk_format, const VkPipelineColorBlendAttachmentState& vk_pipeline_color_blend_state) {
    horizon_profile();
    vk_color_formats.push_back(vk_format);
    vk_pipeline_color_blend_attachment_states.push_back(vk_pipeline_color_blend_state);
    return *this;
}

config_pipeline_t& config_pipeline_t::set_depth(VkFormat vk_format, const VkPipelineDepthStencilStateCreateInfo& vk_pipeline_color_blend_state) {
    horizon_profile();
    vk_depth_format = vk_format;
    this->vk_pipeline_depth_stencil_state_create_info = vk_pipeline_color_blend_state;
    return *this;
}

config_pipeline_t& config_pipeline_t::add_dynamic_state(const VkDynamicState& vk_dynamic_state) {
    horizon_profile();
    vk_dynamic_states.push_back(vk_dynamic_state);
    return *this;
}

config_pipeline_t& config_pipeline_t::add_vertex_input_binding_description(uint32_t vk_binding, uint32_t vk_stride, VkVertexInputRate vk_input_rate) {
    horizon_profile();
    VkVertexInputBindingDescription vk_vertex_input_binding_description{};
    vk_vertex_input_binding_description.binding = vk_binding;
    vk_vertex_input_binding_description.stride = vk_stride;
    vk_vertex_input_binding_description.inputRate = vk_input_rate;
    vk_vertex_input_binding_descriptions.push_back(vk_vertex_input_binding_description);
    return *this;
}

config_pipeline_t& config_pipeline_t::add_vertex_input_attribute_description(uint32_t vk_binding, uint32_t vk_location, VkFormat vk_format, uint32_t vk_offset) {
    horizon_profile();
    VkVertexInputAttributeDescription vk_vertex_input_attribute_description{};
    vk_vertex_input_attribute_description.binding = vk_binding;
    vk_vertex_input_attribute_description.format = vk_format;
    vk_vertex_input_attribute_description.location = vk_location;
    vk_vertex_input_attribute_description.offset = vk_offset;
    vk_vertex_input_attribute_descriptions.push_back(vk_vertex_input_attribute_description);
    return *this;
}

config_pipeline_t& config_pipeline_t::set_pipeline_input_assembly_state(const VkPipelineInputAssemblyStateCreateInfo& vk_pipeline_input_assembly_state) {
    horizon_profile();
    this->vk_pipeline_input_assembly_state = vk_pipeline_input_assembly_state;
    return *this;
}

config_pipeline_t& config_pipeline_t::set_pipeline_rasterization_state(const VkPipelineRasterizationStateCreateInfo& vk_pipeline_rasterization_state) {
    horizon_profile();
    this->vk_pipeline_rasterization_state = vk_pipeline_rasterization_state;
    return *this;
}

config_pipeline_t& config_pipeline_t::set_pipeline_multisample_state(const VkPipelineMultisampleStateCreateInfo& vk_pipeline_multisample_state) {
    horizon_profile();
    this->vk_pipeline_multisample_state = vk_pipeline_multisample_state;
    return *this;
}

VkPipelineColorBlendAttachmentState default_color_blend_attachment() {
    horizon_profile();
    VkPipelineColorBlendAttachmentState vk_color_blend_attachment{};
    vk_color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    vk_color_blend_attachment.blendEnable = VK_FALSE;
    vk_color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    vk_color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    vk_color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
    vk_color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    vk_color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    vk_color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
    return vk_color_blend_attachment;
}

update_descriptor_set_t& update_descriptor_set_t::push_buffer_write(uint32_t binding, const buffer_descriptor_info_t& info, uint32_t count) {
    horizon_profile();
    internal::descriptor_set_t& descriptor_set = utils::assert_and_get_data<internal::descriptor_set_t>(handle, context._descriptor_sets);
    VkWriteDescriptorSet vk_write{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    vk_write.dstBinding = binding;
    vk_write.descriptorCount = count;
    vk_write.dstSet = descriptor_set;
    VkDescriptorBufferInfo *vk_buffer_info = new VkDescriptorBufferInfo;
    vk_buffer_info->buffer = utils::assert_and_get_data<internal::buffer_t>(info.handle_buffer, context._buffers);
    vk_buffer_info->offset = info.vk_offset;
    vk_buffer_info->range = info.vk_range;
    
    internal::descriptor_set_layout_t& descriptor_set_layout = utils::assert_and_get_data<internal::descriptor_set_layout_t>(descriptor_set.config.handle_descriptor_set_layout, context._descriptor_set_layouts);
    auto itr = std::find_if(descriptor_set_layout.config.vk_descriptor_set_layout_bindings.begin(),
                            descriptor_set_layout.config.vk_descriptor_set_layout_bindings.end(),
                            [binding](const VkDescriptorSetLayoutBinding& layout_binding) {
                                return binding == layout_binding.binding;
                            });
    assert(itr != descriptor_set_layout.config.vk_descriptor_set_layout_bindings.end());
    vk_write.descriptorType = itr->descriptorType;
    vk_write.pBufferInfo = vk_buffer_info;
    vk_writes.push_back(vk_write);
    return *this;
}

update_descriptor_set_t& update_descriptor_set_t::push_image_write(uint32_t binding, const image_descriptor_info_t& info, uint32_t count) {
    horizon_profile();
    internal::descriptor_set_t& descriptor_set = utils::assert_and_get_data<internal::descriptor_set_t>(handle, context._descriptor_sets);
    VkWriteDescriptorSet vk_write{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    vk_write.dstBinding = binding;
    vk_write.descriptorCount = count;
    vk_write.dstSet = descriptor_set;
    VkDescriptorImageInfo *vk_image_info = new VkDescriptorImageInfo;
    vk_image_info->sampler = utils::assert_and_get_data<internal::sampler_t>(info.handle_sampler, context._samplers);
    vk_image_info->imageView = utils::assert_and_get_data<internal::image_view_t>(info.handle_image_view, context._image_views);
    vk_image_info->imageLayout = info.vk_image_layout;

    internal::descriptor_set_layout_t& descriptor_set_layout = utils::assert_and_get_data<internal::descriptor_set_layout_t>(descriptor_set.config.handle_descriptor_set_layout, context._descriptor_set_layouts);
    auto itr = std::find_if(descriptor_set_layout.config.vk_descriptor_set_layout_bindings.begin(),
                            descriptor_set_layout.config.vk_descriptor_set_layout_bindings.end(),
                            [binding](const VkDescriptorSetLayoutBinding& layout_binding) {
                                return binding == layout_binding.binding;
                            });
    assert(itr != descriptor_set_layout.config.vk_descriptor_set_layout_bindings.end());
    vk_write.descriptorType = itr->descriptorType;
    vk_write.pImageInfo = vk_image_info;
    vk_writes.push_back(vk_write);
    return *this;
}

void update_descriptor_set_t::commit() {
    horizon_profile();
    vkUpdateDescriptorSets(context._vkb_device, vk_writes.size(), vk_writes.data(), 0, nullptr);
    for (auto& vk_write : vk_writes) {
        if (vk_write.pBufferInfo) delete vk_write.pBufferInfo;
        if (vk_write.pImageInfo) delete vk_write.pImageInfo;
    }    
    vk_writes.clear();
    horizon_trace("updated descriptor set {}", handle);
}

struct volk_initializer_t {
    volk_initializer_t() {
        horizon_profile();
        if (volkInitialize() != VK_SUCCESS) {
            horizon_error("Failed to initialize volk");
            std::terminate();
        }
    }
};

static volk_initializer_t volk_initializer{};

context_t::context_t(const bool enable_validation) : _validation(enable_validation) {
    horizon_profile();
    create_instance();
    create_device();
    create_allocator();
    create_descriptor_pool();
}

context_t::~context_t() {
    horizon_profile();
    vkDeviceWaitIdle(_vkb_device);
    for (auto& [handle, commandbuffer] : _commandbuffers) {
        horizon_warn("forgot to clear commandbuffer with handle: {}", handle);
        vkFreeCommandBuffers(_vkb_device, utils::assert_and_get_data<internal::command_pool_t>(commandbuffer.config.handle_command_pool, _command_pools), 1, &commandbuffer.vk_commandbuffer);
    }
    for (auto& [handle, command_pool] : _command_pools) {
        horizon_warn("forgot to clear command pool with handle: {}", handle);
        vkDestroyCommandPool(_vkb_device, command_pool, nullptr);
    }
    for (auto& [handle, semaphore] : _semaphores) {
        horizon_warn("forgot to clear semaphore with handle: {}", handle);
        vkDestroySemaphore(_vkb_device, semaphore, nullptr);
    }
    for (auto& [handle, fence] : _fences) {
        horizon_warn("forgot to clear fence with handle: {}", handle);
        vkDestroyFence(_vkb_device, fence, nullptr);
    }
    for (auto& [handle, pipeline_layout] : _pipeline_layouts) {
        horizon_warn("forgot to clear pipeline layout with handle: {}", handle);
        vkDestroyPipelineLayout(_vkb_device, pipeline_layout, nullptr);
    }
    for (auto& [handle, pipeline] : _pipelines) {
        horizon_warn("forgot to clear pipeline with handle: {}", handle);
        vkDestroyPipeline(_vkb_device, pipeline, nullptr);
    }
    for (auto& [handle, shader] : _shaders) {
        horizon_warn("forgot to clear shader with handle: {}", handle);
        vkDestroyShaderModule(_vkb_device, shader, nullptr);
    }
    for (auto& [handle, descriptor_set] : _descriptor_sets) {
        horizon_warn("forgot to clear descriptor set with handle: {}", handle);
        vkFreeDescriptorSets(_vkb_device, _vk_descriptor_pool, 1, &descriptor_set.vk_descriptor_set);
    }
    for (auto& [handle, descriptor_set_layout] : _descriptor_set_layouts) {
        horizon_warn("forgot to clear descriptor set layout with handle: {}", handle);
        vkDestroyDescriptorSetLayout(_vkb_device, descriptor_set_layout, nullptr);
    }
    for (auto& [handle, image_view] : _image_views) {
        horizon_warn("forgot to clear image view with handle: {}", handle);
        vkDestroyImageView(_vkb_device, image_view, nullptr);
    }
    for (auto& [handle, image] : _images) if (!image.from_swapchain) {
        horizon_warn("forgot to clear image with handle: {}", handle);
        // if (image.p_data) unmap_image(handle);
        vmaDestroyImage(_vma_allocator, image, image.vma_allocation);
    }
    for (auto& [handle, sampler] : _samplers) {
        horizon_warn("forgot to clear sampler with handle: {}", handle);
        vkDestroySampler(_vkb_device, sampler, nullptr);
    }
    for (auto& [handle, buffer] : _buffers) {
        horizon_warn("forgot to clear buffer with handle: {}", handle);
        if (buffer.p_data) unmap_buffer(handle);
        vmaDestroyBuffer(_vma_allocator, buffer, buffer.vma_allocation);
    }
    for (auto& [handle, swapchain] : _swapchains) {
        horizon_warn("forgot to clear swapchain with handle: {}", handle);    
        vkDestroySwapchainKHR(_vkb_device, swapchain.vk_swapchain, nullptr);
        vkDestroySurfaceKHR(_vkb_instance, swapchain.vk_surface, nullptr);
    }
    vkDestroyDescriptorPool(_vkb_device, _vk_descriptor_pool, nullptr);
    vmaDestroyAllocator(_vma_allocator);
    vkb::destroy_device(_vkb_device);
    vkb::destroy_instance(_vkb_instance);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT vk_message_severity,
    VkDebugUtilsMessageTypeFlagsEXT vk_message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* p_vk_callback_data,
    void* p_user_data) {
    horizon_profile();
    switch (vk_message_severity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: 
            horizon_trace("{}", p_vk_callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: 
            horizon_info("{}", p_vk_callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: 
            horizon_warn("{}", p_vk_callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: 
            horizon_error("{}", p_vk_callback_data->pMessage);
            break;
        default:
            horizon_error("unknown message severity");
            std::terminate();
    }
    return VK_FALSE;
}

void context_t::create_instance() {
    horizon_profile();
    vkb::InstanceBuilder vkb_instance_builder{};
    vkb_instance_builder.set_debug_callback(debug_callback)
                       .desire_api_version(VK_API_VERSION_1_3)
                       .enable_extension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)
                       .request_validation_layers(_validation);
    auto result = vkb_instance_builder.build();
    check(result, "Failed to create instance");
    _vkb_instance = result.value();
    volkLoadInstance(_vkb_instance);
    horizon_trace("created instance");
}

void context_t::create_device() {
    horizon_profile();
    vkb::PhysicalDeviceSelector vkb_physical_device_selector{ _vkb_instance };
    vkb_physical_device_selector.add_required_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    vkb_physical_device_selector.add_required_extension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    vkb_physical_device_selector.add_required_extension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
    vkb_physical_device_selector.add_required_extension(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
    vkb_physical_device_selector.add_required_extension(VK_KHR_BIND_MEMORY_2_EXTENSION_NAME);
    vkb_physical_device_selector.add_required_extension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    VkPhysicalDeviceScalarBlockLayoutFeatures vk_physical_device_scalar_block_layout_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES,
        .scalarBlockLayout = VK_TRUE,
    };
    vkb_physical_device_selector.add_required_extension_features<VkPhysicalDeviceScalarBlockLayoutFeatures>(vk_physical_device_scalar_block_layout_features);
    VkPhysicalDeviceBufferDeviceAddressFeatures vk_physical_device_buffer_device_address_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
        .bufferDeviceAddress = VK_TRUE,
        .bufferDeviceAddressCaptureReplay = VK_FALSE,
        .bufferDeviceAddressMultiDevice = VK_FALSE
    };
    vkb_physical_device_selector.add_required_extension_features<VkPhysicalDeviceBufferDeviceAddressFeatures>(vk_physical_device_buffer_device_address_features);
    VkPhysicalDeviceDynamicRenderingFeaturesKHR vk_physical_device_dynamic_rendering_features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES };
    vk_physical_device_dynamic_rendering_features.dynamicRendering = VK_TRUE;
    vkb_physical_device_selector.add_required_extension_features<VkPhysicalDeviceDynamicRenderingFeaturesKHR>(vk_physical_device_dynamic_rendering_features);
    // vkb_physical_device_selector.prefer_gpu_device_type(vkb::PreferredDeviceType::);

    // create temp window to get surface information
    core::window_t temp_window{ "temp", 2, 2 };
    VkSurfaceKHR vk_temp_surface;
    {
        VkResult vk_result = glfwCreateWindowSurface(_vkb_instance, temp_window.window(), nullptr, &vk_temp_surface);
        check(vk_result == VK_SUCCESS, "Failed to create surface");
    }
    vkb_physical_device_selector.set_surface(vk_temp_surface);
    {
        auto result = vkb_physical_device_selector.select(vkb::DeviceSelectionMode::only_fully_suitable);
        check(result, "Failed to pick suitable device");
        _vkb_physical_device = result.value();
        horizon_info("{}", _vkb_physical_device.name);
    }
    vkb::DeviceBuilder vkb_device_builder{ _vkb_physical_device };
    {
        auto result = vkb_device_builder.build();
        check(result, "Failed to create logical device");
        _vkb_device = result.value();
    }
    {
        auto result = _vkb_device.get_queue(vkb::QueueType::graphics);
        check(result, "Failed to get graphics queue");
        _graphics_queue.vk_queue = result.value();
    }
    {
        auto result = _vkb_device.get_queue_index(vkb::QueueType::graphics);
        check(result, "Failed to get graphics queue index");
        _graphics_queue.vk_index = result.value();
    }
    {
        auto result = _vkb_device.get_queue(vkb::QueueType::present);
        check(result, "Failed to get present queue");
        _present_queue.vk_queue = result.value();
    }
    {
        auto result = _vkb_device.get_queue_index(vkb::QueueType::present);
        check(result, "Failed to get present queue index");
        _present_queue.vk_index = result.value();
    }
    vkDestroySurfaceKHR(_vkb_instance, vk_temp_surface, nullptr);
    volkLoadDevice(_vkb_device);

    horizon_trace("created device");
}

void context_t::create_allocator() {
    horizon_profile();
    VmaVulkanFunctions vma_vulkan_functions = {
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
        .vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
        .vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
        .vkAllocateMemory = vkAllocateMemory,
        .vkFreeMemory = vkFreeMemory,
        .vkMapMemory = vkMapMemory,
        .vkUnmapMemory = vkUnmapMemory,
        .vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges,
        .vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges,
        .vkBindBufferMemory = vkBindBufferMemory,
        .vkBindImageMemory = vkBindImageMemory,
        .vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements,
        .vkGetImageMemoryRequirements = vkGetImageMemoryRequirements,
        .vkCreateBuffer = vkCreateBuffer,
        .vkDestroyBuffer = vkDestroyBuffer,
        .vkCreateImage = vkCreateImage,
        .vkDestroyImage = vkDestroyImage,
        .vkCmdCopyBuffer = vkCmdCopyBuffer,
        .vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2KHR,
        .vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2KHR,
        .vkBindBufferMemory2KHR = vkBindBufferMemory2KHR,
        .vkBindImageMemory2KHR = vkBindImageMemory2KHR,
        .vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2KHR,
        .vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements,
        .vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements,
    };
    VmaAllocatorCreateInfo vma_allocator_create_info{
        .flags = {},
        .physicalDevice = _vkb_physical_device,
        .device = _vkb_device,
        .pVulkanFunctions = &vma_vulkan_functions,
        .instance = _vkb_instance,
        .vulkanApiVersion = VK_API_VERSION_1_3,
    };
    VkResult vk_result = vmaCreateAllocator(&vma_allocator_create_info, &_vma_allocator);
    check(vk_result == VK_SUCCESS, "Failed to create allocator");
    horizon_trace("created allocator");
}

void context_t::create_descriptor_pool() {
    horizon_profile();
    std::vector<VkDescriptorPoolSize> vk_pool_sizes{};
    {
        VkDescriptorPoolSize vk_pool_size{};
        vk_pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        vk_pool_size.descriptorCount = 1000;
        vk_pool_sizes.push_back(vk_pool_size);
    }
    {
        VkDescriptorPoolSize vk_pool_size{};
        vk_pool_size.type = VK_DESCRIPTOR_TYPE_SAMPLER;
        vk_pool_size.descriptorCount = 1000;
        vk_pool_sizes.push_back(vk_pool_size);
    }
    {
        VkDescriptorPoolSize vk_pool_size{};
        vk_pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        vk_pool_size.descriptorCount = 1000;
        vk_pool_sizes.push_back(vk_pool_size);
    }
    {
        VkDescriptorPoolSize vk_pool_size{};
        vk_pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        vk_pool_size.descriptorCount = 1000;
        vk_pool_sizes.push_back(vk_pool_size);
    }
    {
        VkDescriptorPoolSize vk_pool_size{};
        vk_pool_size.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        vk_pool_size.descriptorCount = 1000;
        vk_pool_sizes.push_back(vk_pool_size);
    }
    {
        VkDescriptorPoolSize vk_pool_size{};
        vk_pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        vk_pool_size.descriptorCount = 1000;
        vk_pool_sizes.push_back(vk_pool_size);
    }
    VkDescriptorPoolCreateInfo vk_descriptor_pool_create_info{};
    vk_descriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    vk_descriptor_pool_create_info.poolSizeCount = vk_pool_sizes.size();
    vk_descriptor_pool_create_info.pPoolSizes = vk_pool_sizes.data();
    vk_descriptor_pool_create_info.maxSets = 1000 * 7;
    vk_descriptor_pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    VkResult vk_result = vkCreateDescriptorPool(_vkb_device, &vk_descriptor_pool_create_info, nullptr, &_vk_descriptor_pool);
    check(vk_result == VK_SUCCESS, "Failed to create descriptor pool");
    horizon_trace("created descriptor pool");
}

handle_swapchain_t context_t::create_swapchain(const core::window_t& window) {
    horizon_profile();
    internal::swapchain_t swapchain {};
    {
        VkResult vk_result = glfwCreateWindowSurface(_vkb_instance, window.window(), nullptr, &swapchain.vk_surface);
        check(vk_result == VK_SUCCESS, "Failed to create surface");
    }
    {
        vkb::SwapchainBuilder vkb_swapchain_builder{ _vkb_device, swapchain.vk_surface };
        auto result = vkb_swapchain_builder.build();
        check(result, "Failed to create swapchain");
        swapchain.vk_swapchain = result.value();
    }
    std::vector<VkImage> vk_images;
    {
        auto result = swapchain.vk_swapchain.get_images();
        check(result, "Failed to get swapchain images");
        vk_images = result.value();
    }
    for (auto vk_image : vk_images) {
        config_image_t config_image{};
        config_image.vk_width = swapchain.vk_swapchain.extent.width;
        config_image.vk_height = swapchain.vk_swapchain.extent.height;
        config_image.vk_depth = 1;
        config_image.vk_type = VK_IMAGE_TYPE_2D;
        config_image.vk_format = swapchain.vk_swapchain.image_format;
        config_image.vk_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        config_image.vk_initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        config_image.vk_mips = 1;
        config_image.vk_array_layers = 1;
        config_image.vk_sample_count = VK_SAMPLE_COUNT_1_BIT;
        internal::image_t image{ .config = config_image };
        image.vk_image = vk_image;
        image.from_swapchain = true;

        handle_image_t handle_image = utils::create_and_insert_new_handle<handle_image_t>(_images, image);

        handle_image_view_t handle_image_view = create_image_view({ .handle_image = handle_image });        

        swapchain.handle_images.push_back(handle_image);
        swapchain.handle_image_views.push_back(handle_image_view);
    }
    handle_swapchain_t handle = utils::create_and_insert_new_handle<handle_swapchain_t>(_swapchains, swapchain);
    horizon_trace("created swapchain");
    return handle;
}

void context_t::destroy_swapchain(handle_swapchain_t handle) {
    horizon_profile();
    internal::swapchain_t& swapchain = utils::assert_and_get_data<internal::swapchain_t>(handle, _swapchains);
    vkDestroySwapchainKHR(_vkb_device, swapchain.vk_swapchain, nullptr);
    vkDestroySurfaceKHR(_vkb_instance, swapchain.vk_surface, nullptr);
    _swapchains.erase(handle);
}

std::vector<handle_image_t> context_t::get_swapchain_images(handle_swapchain_t handle) {
    horizon_profile();
    internal::swapchain_t& swapchain = utils::assert_and_get_data<internal::swapchain_t>(handle, _swapchains);
    return swapchain.handle_images;
}

std::vector<handle_image_view_t> context_t::get_swapchain_image_views(handle_swapchain_t handle) {
    horizon_profile();
    internal::swapchain_t& swapchain = utils::assert_and_get_data<internal::swapchain_t>(handle, _swapchains);
    return swapchain.handle_image_views;
}

std::optional<uint32_t> context_t::get_swapchain_next_image_index(handle_swapchain_t handle, handle_semaphore_t handle_semaphore, handle_fence_t handle_fence) {
    horizon_profile();
    internal::swapchain_t& swapchain = utils::assert_and_get_data<internal::swapchain_t>(handle, _swapchains);
    VkSemaphore vk_semaphore = VK_NULL_HANDLE;
    VkFence vk_fence = VK_NULL_HANDLE;
    if (handle_semaphore != null_handle) {
        vk_semaphore = utils::assert_and_get_data<internal::semaphore_t>(handle_semaphore, _semaphores);
    }
    if (handle_fence != null_handle) {
        vk_fence = utils::assert_and_get_data<internal::fence_t>(handle_fence, _fences);
    }
    uint32_t next_image;
    {
        VkResult vk_result = vkAcquireNextImageKHR(_vkb_device, swapchain.vk_swapchain, UINT64_MAX, vk_semaphore, vk_fence, &next_image);
        if (vk_result == VK_ERROR_OUT_OF_DATE_KHR) return std::nullopt;
        check(vk_result == VK_SUCCESS || vk_result == VK_SUBOPTIMAL_KHR, "Failed to acquire swapchain image");
    }
    return next_image;
}

void context_t::present_swapchain(handle_swapchain_t handle, uint32_t image_index, std::vector<handle_semaphore_t> handle_semaphores) {
    horizon_profile();
    internal::swapchain_t& swapchain = utils::assert_and_get_data<internal::swapchain_t>(handle, _swapchains);
    VkSemaphore *vk_semaphores = reinterpret_cast<VkSemaphore *>(alloca(handle_semaphores.size() * sizeof(VkSemaphore)));
    for (size_t i = 0; i < handle_semaphores.size(); i++) {
        vk_semaphores[i] = utils::assert_and_get_data<internal::semaphore_t>(handle_semaphores[i], _semaphores);
    }
    VkResult vk_result;
    VkPresentInfoKHR vk_present_info{ .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    vk_present_info.waitSemaphoreCount = handle_semaphores.size();
    vk_present_info.pWaitSemaphores = vk_semaphores;
    vk_present_info.swapchainCount = 1;
    vk_present_info.pSwapchains = &swapchain.vk_swapchain.swapchain;
    vk_present_info.pImageIndices = &image_index;
    vk_present_info.pResults = &vk_result;

    {
        VkResult vk_result = vkQueuePresentKHR(_present_queue.vk_queue, &vk_present_info);
        check(vk_result == VK_SUCCESS, "Failed to present");
    }
    check(vk_result == VK_SUCCESS, "Failed to present");
}

handle_buffer_t context_t::create_buffer(const config_buffer_t& config) {
    horizon_profile();
    internal::buffer_t buffer{ .config = config };

    VkBufferCreateInfo vk_buffer_create_info{ .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    vk_buffer_create_info.size = config.vk_size;
    vk_buffer_create_info.usage = config.vk_buffer_usage_flags;

    VmaAllocationCreateInfo vma_allocation_create_info{};
    vma_allocation_create_info.usage = config.vma_memory_usage;
    vma_allocation_create_info.flags = config.vma_allocation_create_flags;

    VkResult vk_result = vmaCreateBuffer(_vma_allocator, &vk_buffer_create_info, &vma_allocation_create_info, &buffer.vk_buffer, &buffer.vma_allocation, nullptr);
    check(vk_result == VK_SUCCESS, "Failed to create buffer");

    handle_buffer_t handle = utils::create_and_insert_new_handle<handle_buffer_t>(_buffers, buffer);
    horizon_trace("created buffer");
    return handle;
}

void context_t::destroy_buffer(handle_buffer_t handle) {
    horizon_profile();
    internal::buffer_t& buffer = utils::assert_and_get_data<internal::buffer_t>(handle, _buffers);
    vmaDestroyBuffer(_vma_allocator, buffer, buffer.vma_allocation);
    _buffers.erase(handle);
}

void *context_t::map_buffer(handle_buffer_t handle) {
    horizon_profile();
    internal::buffer_t& buffer = utils::assert_and_get_data<internal::buffer_t>(handle, _buffers);
    if (buffer.p_data) return buffer.p_data;
    vmaMapMemory(_vma_allocator, buffer.vma_allocation, &buffer.p_data);
    return buffer.p_data;
}

void context_t::unmap_buffer(handle_buffer_t handle) {
    horizon_profile();
    internal::buffer_t& buffer = utils::assert_and_get_data<internal::buffer_t>(handle, _buffers);
    if (!buffer.p_data) return;
    vmaUnmapMemory(_vma_allocator, buffer.vma_allocation);
    buffer.p_data = nullptr;
}

internal::buffer_t& context_t::get_buffer(handle_buffer_t handle) {
    horizon_profile();
    return utils::assert_and_get_data<internal::buffer_t>(handle, _buffers);
}

handle_sampler_t context_t::create_sampler(const config_sampler_t& config) {
    horizon_profile();
    
    internal::sampler_t sampler{ .config = config };
    VkSamplerCreateInfo vk_sampler_create_info{ .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    vk_sampler_create_info.flags;
    vk_sampler_create_info.magFilter = config.vk_mag_filter;
    vk_sampler_create_info.minFilter = config.vk_min_filter;
    vk_sampler_create_info.mipmapMode = config.vk_mipmap_mode;
    vk_sampler_create_info.addressModeU = config.vk_address_mode_u;
    vk_sampler_create_info.addressModeV = config.vk_address_mode_v;
    vk_sampler_create_info.addressModeW = config.vk_address_mode_w;
    vk_sampler_create_info.mipLodBias = config.vk_mip_lod_bias;
    vk_sampler_create_info.anisotropyEnable = config.vk_anisotropy_enable;
    vk_sampler_create_info.maxAnisotropy = config.vk_max_anisotropy;
    vk_sampler_create_info.compareEnable = config.vk_compare_enable;
    vk_sampler_create_info.compareOp = config.vk_compare_op;
    vk_sampler_create_info.minLod = config.vk_min_lod;
    vk_sampler_create_info.maxLod = config.vk_max_lod;
    vk_sampler_create_info.borderColor = config.vk_border_color;
    vk_sampler_create_info.unnormalizedCoordinates = config.vk_unnormalized_coordinates;

    VkResult vk_result = vkCreateSampler(_vkb_device, &vk_sampler_create_info, nullptr, &sampler.vk_sampler);
    check(vk_result == VK_SUCCESS, "Failed to create sampler");

    handle_sampler_t handle = utils::create_and_insert_new_handle<handle_sampler_t>(_samplers, sampler);
    horizon_trace("created sampler");
    return handle;
}

void context_t::destroy_sampler(handle_sampler_t handle) {
    horizon_profile();
    internal::sampler_t& sampler = utils::assert_and_get_data<internal::sampler_t>(handle, _samplers);
    vkDestroySampler(_vkb_device, sampler, nullptr);
    _samplers.erase(handle);
}

internal::sampler_t& context_t::get_sampler(handle_sampler_t handle) {
    horizon_profile();
    return utils::assert_and_get_data<internal::sampler_t>(handle, _samplers);
}

handle_image_t context_t::create_image(const config_image_t& config) {
    horizon_profile();
    VkImageCreateInfo vk_image_create_info{ .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    vk_image_create_info.imageType = config.vk_type;
    vk_image_create_info.extent = { config.vk_width, config.vk_height, config.vk_depth };
    if (config.vk_mips == vk_auto_calculate_mip_levels) {
        VkImageFormatProperties image_format_properties{};
        vkGetPhysicalDeviceImageFormatProperties(_vkb_physical_device, VK_FORMAT_R8G8B8A8_SRGB, VkImageType::VK_IMAGE_TYPE_2D, VkImageTiling::VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 0, &image_format_properties);
        vk_image_create_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(std::max(config.vk_width, config.vk_height), config.vk_depth)))) + 1;  // this might be wrong ?
        vk_image_create_info.mipLevels = std::min(image_format_properties.maxMipLevels, vk_image_create_info.mipLevels); 
    } else {
        vk_image_create_info.mipLevels = config.vk_mips;
    }
    vk_image_create_info.arrayLayers = config.vk_array_layers;
    vk_image_create_info.format = config.vk_format;
    vk_image_create_info.tiling = config.vk_tiling;
    vk_image_create_info.initialLayout = config.vk_initial_layout;
    vk_image_create_info.usage = config.vk_usage;
    vk_image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vk_image_create_info.samples = config.vk_sample_count;
    vk_image_create_info.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    
    internal::image_t image{ .config = config };
    image.config.vk_mips = vk_image_create_info.mipLevels;

    VmaAllocationCreateInfo vma_allocation_create_info{};
    vma_allocation_create_info.usage = config.vma_memory_usage;
    vma_allocation_create_info.flags = config.vma_allocation_create_flags;

    VkResult vk_result = vmaCreateImage(_vma_allocator, &vk_image_create_info, &vma_allocation_create_info, &image.vk_image, &image.vma_allocation, nullptr);
    check(vk_result == VK_SUCCESS, "Failed to create image");

    handle_image_t handle = utils::create_and_insert_new_handle<handle_image_t>(_images, image);
    horizon_trace("created image");
    return handle;
}

void context_t::destroy_image(handle_image_t handle) {
    horizon_profile();
    internal::image_t& image = utils::assert_and_get_data<internal::image_t>(handle, _images);
    vmaDestroyImage(_vma_allocator, image, image.vma_allocation);
    _images.erase(handle);
}

handle_image_view_t context_t::create_image_view(const config_image_view_t& config) {
    horizon_profile();
    internal::image_t image = utils::assert_and_get_data<internal::image_t>(config.handle_image, _images);

    internal::image_view_t image_view{ .config = config };

    VkImageViewType vk_image_view_type;
    if (config.vk_image_view_type == vk_auto_image_view_type) {
        if (image.config.vk_type == VkImageType::VK_IMAGE_TYPE_1D) vk_image_view_type = VkImageViewType::VK_IMAGE_VIEW_TYPE_1D;
        if (image.config.vk_type == VkImageType::VK_IMAGE_TYPE_2D) vk_image_view_type = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
        if (image.config.vk_type == VkImageType::VK_IMAGE_TYPE_3D) vk_image_view_type = VkImageViewType::VK_IMAGE_VIEW_TYPE_3D;
    } else {
        vk_image_view_type = config.vk_image_view_type;
    }

    VkImageViewCreateInfo vk_image_view_create_info{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    vk_image_view_create_info.image = image;
    vk_image_view_create_info.viewType = vk_image_view_type;
    vk_image_view_create_info.format = config.vk_format == vk_auto_image_format ? image.config.vk_format : config.vk_format;
    vk_image_view_create_info.subresourceRange.aspectMask = utils::get_image_aspect(image.config.vk_format);
    vk_image_view_create_info.subresourceRange.baseMipLevel = config.vk_base_mip_level;   // default base mip is 0 automatically, and for custom it already has the value
    vk_image_view_create_info.subresourceRange.levelCount = config.vk_mips == vk_auto_mips ? image.config.vk_mips : config.vk_mips;
    vk_image_view_create_info.subresourceRange.baseArrayLayer = config.vk_base_array_layer;  // default base array layer is 0 automatically, and for custom it already has the value
    vk_image_view_create_info.subresourceRange.layerCount = config.vk_layers == vk_auto_layers ? image.config.vk_array_layers : config.vk_layers;

    VkResult vk_result = vkCreateImageView(_vkb_device, &vk_image_view_create_info, nullptr, &image_view.vk_image_view);
    check(vk_result == VK_SUCCESS, "Failed to create image view");

    handle_image_view_t handle = utils::create_and_insert_new_handle<handle_image_view_t>(_image_views, image_view);
    horizon_trace("created image view");
    return handle;
}

void context_t::destroy_image_view(handle_image_view_t handle) {
    horizon_profile();
    internal::image_view_t& image_view = utils::assert_and_get_data<internal::image_view_t>(handle, _image_views);
    vkDestroyImageView(_vkb_device, image_view, nullptr);
    _image_views.erase(handle);
}

internal::image_view_t& context_t::get_image_view(handle_image_view_t handle) {
    horizon_profile();
    return utils::assert_and_get_data<internal::image_view_t>(handle, _image_views);
}

handle_descriptor_set_layout_t context_t::create_descriptor_set_layout(const config_descriptor_set_layout_t& config) {
    horizon_profile();
    internal::descriptor_set_layout_t descriptor_set_layout{ .config = config };

    VkDescriptorSetLayoutCreateInfo vk_descriptor_set_layout_create_info{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    vk_descriptor_set_layout_create_info.bindingCount = config.vk_descriptor_set_layout_bindings.size();
    vk_descriptor_set_layout_create_info.pBindings = config.vk_descriptor_set_layout_bindings.data();

    VkResult vk_result = vkCreateDescriptorSetLayout(_vkb_device, &vk_descriptor_set_layout_create_info, nullptr, &descriptor_set_layout.vk_descriptor_set_layout);
    check(vk_result == VK_SUCCESS, "Failed to create descriptor set layout");

    handle_descriptor_set_layout_t handle = utils::create_and_insert_new_handle<handle_descriptor_set_layout_t>(_descriptor_set_layouts, descriptor_set_layout);
    horizon_trace("created descriptor set layout");
    return handle;
}

void context_t::destroy_descriptor_set_layout(handle_descriptor_set_layout_t handle) {
    horizon_profile();
    internal::descriptor_set_layout_t& descriptor_set_layout = utils::assert_and_get_data<internal::descriptor_set_layout_t>(handle, _descriptor_set_layouts);
    vkDestroyDescriptorSetLayout(_vkb_device, descriptor_set_layout, nullptr);
    _descriptor_set_layouts.erase(handle);
}

internal::descriptor_set_layout_t& context_t::get_descriptor_set_layout(handle_descriptor_set_layout_t handle) {
    horizon_profile();
    return utils::assert_and_get_data<internal::descriptor_set_layout_t>(handle, _descriptor_set_layouts);
}

handle_descriptor_set_t context_t::allocate_descriptor_set(const config_descriptor_set_t& config) {
    horizon_profile();
    internal::descriptor_set_t descriptor_set{ .config = config };

    internal::descriptor_set_layout_t& descriptor_set_layout = utils::assert_and_get_data<internal::descriptor_set_layout_t>(config.handle_descriptor_set_layout, _descriptor_set_layouts);

    VkDescriptorSetAllocateInfo vk_descriptor_set_allocate_info{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    vk_descriptor_set_allocate_info.descriptorPool = _vk_descriptor_pool;
    vk_descriptor_set_allocate_info.descriptorSetCount = 1;
    vk_descriptor_set_allocate_info.pSetLayouts = &descriptor_set_layout.vk_descriptor_set_layout;

    VkResult vk_result = vkAllocateDescriptorSets(_vkb_device, &vk_descriptor_set_allocate_info, &descriptor_set.vk_descriptor_set);
    check(vk_result == VK_SUCCESS, "Failed to allocate descriptor set");

    handle_descriptor_set_t handle = utils::create_and_insert_new_handle<handle_descriptor_set_t>(_descriptor_sets, descriptor_set);
    horizon_trace("allocated descriptor set");
    return handle;
}

void context_t::free_descriptor_set(handle_descriptor_set_t handle) {
    horizon_profile();
    internal::descriptor_set_t& descriptor_set = utils::assert_and_get_data<internal::descriptor_set_t>(handle, _descriptor_sets);
    VkResult vk_result = vkFreeDescriptorSets(_vkb_device, _vk_descriptor_pool, 1, &descriptor_set.vk_descriptor_set);
    check(vk_result == VK_SUCCESS, "Failed to free descriptor set");
    _descriptor_sets.erase(handle);
}

update_descriptor_set_t context_t::update_descriptor_set(handle_descriptor_set_t handle) {
    horizon_profile();
    return { *this, handle };
}

internal::descriptor_set_t& context_t::get_descriptor_set(handle_descriptor_set_t handle) {
    horizon_profile();
    return utils::assert_and_get_data<internal::descriptor_set_t>(handle, _descriptor_sets);
}

handle_pipeline_layout_t context_t::create_pipeline_layout(const config_pipeline_layout_t& config) {
    horizon_profile();
    internal::pipeline_layout_t pipeline_layout{ .config = config };
    VkDescriptorSetLayout *vk_descriptor_set_layouts = reinterpret_cast<VkDescriptorSetLayout *>(alloca(config.handle_descriptor_set_layouts.size() * sizeof(VkDescriptorSetLayout)));
    for (size_t i = 0; i < config.handle_descriptor_set_layouts.size(); i++) {
        internal::descriptor_set_layout_t& descriptor_set_layout = utils::assert_and_get_data<internal::descriptor_set_layout_t>(config.handle_descriptor_set_layouts[i], _descriptor_set_layouts);
        vk_descriptor_set_layouts[i] = descriptor_set_layout;
    }
    
    VkPipelineLayoutCreateInfo vk_pipeline_layout_create_info{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    vk_pipeline_layout_create_info.setLayoutCount = config.handle_descriptor_set_layouts.size();
    vk_pipeline_layout_create_info.pSetLayouts = vk_descriptor_set_layouts;
    vk_pipeline_layout_create_info.pushConstantRangeCount = config.vk_push_constant_ranges.size();
    vk_pipeline_layout_create_info.pPushConstantRanges = config.vk_push_constant_ranges.data();
    {
        VkResult vk_result = vkCreatePipelineLayout(_vkb_device, &vk_pipeline_layout_create_info, nullptr, &pipeline_layout.vk_pipeline_layout);
        check(vk_result == VK_SUCCESS, "Failed to create pipeline layout");
    }
    handle_pipeline_layout_t handle = utils::create_and_insert_new_handle<handle_pipeline_layout_t>(_pipeline_layouts, pipeline_layout);
    horizon_trace("created pipeline layout");
    return handle;
}

void context_t::destroy_pipeline_layout(handle_pipeline_layout_t handle) {
    horizon_profile();
    internal::pipeline_layout_t& pipeline_layout = utils::assert_and_get_data<internal::pipeline_layout_t>(handle, _pipeline_layouts);
    vkDestroyPipelineLayout(_vkb_device, pipeline_layout, nullptr);
    _pipeline_layouts.erase(handle);
}
handle_shader_t context_t::create_shader(const config_shader_t& config) {
    horizon_profile();

    shaderc_shader_kind shaderc_kind;
    switch (config.type) {
        case shader_type_t::e_vertex:
            shaderc_kind = shaderc_vertex_shader;
            break;
        case shader_type_t::e_fragment:
            shaderc_kind = shaderc_fragment_shader;
            break;
        case shader_type_t::e_compute:
            shaderc_kind = shaderc_compute_shader;
            break;
        default:
            horizon_error("unknown shader type");
            std::terminate();
    }

    static shaderc::Compiler shaderc_compiler{};
    static shaderc::CompileOptions shaderc_compile_options{};
    static shaderc_util::FileFinder file_finder{};
    static bool once = []() {
        #ifndef NDEBUG
        shaderc_compile_options.SetOptimizationLevel(shaderc_optimization_level_zero);
        shaderc_compile_options.SetGenerateDebugInfo();
        #else
        shaderc_compile_options.SetOptimizationLevel(shaderc_optimization_level_performance);
        #endif
        shaderc_compile_options.SetIncluder(std::make_unique<glslc::FileIncluder>(&file_finder));
        return true;
    }();

    auto preprocess = shaderc_compiler.PreprocessGlsl(config.code, shaderc_kind, config.name.c_str(), shaderc_compile_options);
    std::string preprocessed_code = { preprocess.begin(), preprocess.end() };
    
    auto spirv_shader_module = shaderc_compiler.CompileGlslToSpv(preprocessed_code, shaderc_kind, config.name.c_str(), shaderc_compile_options);
    if (spirv_shader_module.GetCompilationStatus() != shaderc_compilation_status_success) {
        horizon_error("{}", spirv_shader_module.GetErrorMessage());
        std::terminate();
    }
    
    std::vector<uint32_t> spirv_shader_module_code = { spirv_shader_module.begin(), spirv_shader_module.end() };

    VkShaderModuleCreateInfo vk_shader_module_create_info{ .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    vk_shader_module_create_info.codeSize = spirv_shader_module_code.size() * 4;
    vk_shader_module_create_info.pCode = spirv_shader_module_code.data();

    internal::shader_t shader{ .config = config };
    VkResult vk_result = vkCreateShaderModule(_vkb_device, &vk_shader_module_create_info, nullptr, &shader.vk_shader);
    check(vk_result == VK_SUCCESS, "Failed to create shader");

    handle_shader_t handle = utils::create_and_insert_new_handle<handle_shader_t>(_shaders, shader);
    horizon_trace("created shader module");
    return handle;
}

void context_t::destroy_shader(handle_shader_t handle) {
    horizon_profile();
    internal::shader_t& shader = utils::assert_and_get_data<internal::shader_t>(handle, _shaders);
    vkDestroyShaderModule(_vkb_device, shader, nullptr);
    _shaders.erase(handle);
}

handle_pipeline_t context_t::create_compute_pipeline(const config_pipeline_t& config) {
    horizon_profile();
    internal::pipeline_t pipeline{ .vk_pipeline_bind_point = VK_PIPELINE_BIND_POINT_COMPUTE, .config = config };

    assert(config.handle_shaders.size() > 0);

    VkPipelineShaderStageCreateInfo vk_pipeline_shader_stage_create_info{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    vk_pipeline_shader_stage_create_info.pName = "main";
    vk_pipeline_shader_stage_create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    vk_pipeline_shader_stage_create_info.module = utils::assert_and_get_data<internal::shader_t>(config.handle_shaders[0], _shaders);

    VkComputePipelineCreateInfo vk_compute_pipeline_create_info{ .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    vk_compute_pipeline_create_info.layout = utils::assert_and_get_data<internal::pipeline_layout_t>(config.handle_pipeline_layout, _pipeline_layouts);
    vk_compute_pipeline_create_info.stage = vk_pipeline_shader_stage_create_info;
    {
        VkResult vk_result = vkCreateComputePipelines(_vkb_device, VK_NULL_HANDLE, 1, &vk_compute_pipeline_create_info, nullptr, &pipeline.vk_pipeline);
        check(vk_result == VK_SUCCESS, "Failed to create compute pipeline");
    }

    handle_pipeline_t handle = utils::create_and_insert_new_handle<handle_pipeline_t>(_pipelines, pipeline);
    horizon_trace("created compute pipeline");
    return handle;
}

handle_pipeline_t context_t::create_graphics_pipeline(const config_pipeline_t& config) {
    horizon_profile();
    internal::pipeline_t pipeline{ .vk_pipeline_bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS, .config = config };
    
    VkPipelineDynamicStateCreateInfo vk_dynamic_state{ .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    vk_dynamic_state.dynamicStateCount = config.vk_dynamic_states.size();
    vk_dynamic_state.pDynamicStates = config.vk_dynamic_states.data();

    VkPipelineVertexInputStateCreateInfo vk_vertex_input_info{};
    vk_vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vk_vertex_input_info.vertexBindingDescriptionCount = config.vk_vertex_input_binding_descriptions.size();
    vk_vertex_input_info.pVertexBindingDescriptions = config.vk_vertex_input_binding_descriptions.data();
    vk_vertex_input_info.vertexAttributeDescriptionCount = config.vk_vertex_input_attribute_descriptions.size();
    vk_vertex_input_info.pVertexAttributeDescriptions = config.vk_vertex_input_attribute_descriptions.data();

    VkPipelineInputAssemblyStateCreateInfo vk_input_assembly = config.vk_pipeline_input_assembly_state;

    VkPipelineRasterizationStateCreateInfo vk_rasterizer = config.vk_pipeline_rasterization_state;

    VkPipelineMultisampleStateCreateInfo vk_multisampling = config.vk_pipeline_multisample_state;

    VkPipelineColorBlendStateCreateInfo vk_color_blending{};
    vk_color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    vk_color_blending.logicOpEnable = VK_FALSE;
    vk_color_blending.logicOp = VK_LOGIC_OP_COPY; // Optional
    vk_color_blending.attachmentCount = config.vk_pipeline_color_blend_attachment_states.size();
    vk_color_blending.pAttachments = config.vk_pipeline_color_blend_attachment_states.data();
    vk_color_blending.blendConstants[0] = 0.0f; // Optional
    vk_color_blending.blendConstants[1] = 0.0f; // Optional
    vk_color_blending.blendConstants[2] = 0.0f; // Optional
    vk_color_blending.blendConstants[3] = 0.0f; // Optional

    VkPipelineDepthStencilStateCreateInfo vk_depth_stencil = config.vk_pipeline_depth_stencil_state_create_info;

    std::vector<VkPipelineShaderStageCreateInfo> vk_pipeline_shader_stage_create_infos;

    for (auto& handle_shader : config.handle_shaders) {
        internal::shader_t& shader = utils::assert_and_get_data<internal::shader_t>(handle_shader, _shaders);

        VkPipelineShaderStageCreateInfo vk_pipeline_shader_stage_create_info{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        vk_pipeline_shader_stage_create_info.pName = "main";
        switch (shader.config.type) {
            case shader_type_t::e_fragment:
                vk_pipeline_shader_stage_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                break;
            case shader_type_t::e_vertex:
                vk_pipeline_shader_stage_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
                break;
            default:
                horizon_error("shouldnt reach here");
                std::terminate();
        }
        vk_pipeline_shader_stage_create_info.module = shader;
        vk_pipeline_shader_stage_create_infos.push_back(vk_pipeline_shader_stage_create_info);
    }

    VkViewport vk_viewport{};
    vk_viewport.x = 0.0f;
    vk_viewport.y = 0.0f;
    vk_viewport.width = (float)5;
    vk_viewport.height = (float)5;
    vk_viewport.minDepth = 0.0f;
    vk_viewport.maxDepth = 1.0f;

    VkRect2D vk_scissor{};
    vk_scissor.offset = {0, 0};
    vk_scissor.extent = { 5, 5 };

    VkPipelineViewportStateCreateInfo vk_viewport_state{};
    vk_viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vk_viewport_state.viewportCount = 1;
    vk_viewport_state.pViewports = &vk_viewport;
    vk_viewport_state.scissorCount = 1;
    vk_viewport_state.pScissors = &vk_scissor;

    VkPipelineRenderingCreateInfo vk_pipeline_rendering_create{ .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    vk_pipeline_rendering_create.pNext                   = nullptr;
    vk_pipeline_rendering_create.colorAttachmentCount    = config.vk_color_formats.size();
    vk_pipeline_rendering_create.pColorAttachmentFormats = config.vk_color_formats.data();
    vk_pipeline_rendering_create.depthAttachmentFormat   = config.vk_depth_format;
    vk_pipeline_rendering_create.stencilAttachmentFormat = config.vk_depth_format;

    VkGraphicsPipelineCreateInfo vk_pipeline_info{};
    vk_pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    vk_pipeline_info.stageCount = static_cast<uint32_t>(config.handle_shaders.size());
    vk_pipeline_info.pStages = vk_pipeline_shader_stage_create_infos.data();
    vk_pipeline_info.pVertexInputState = &vk_vertex_input_info;
    vk_pipeline_info.pInputAssemblyState = &vk_input_assembly;
    vk_pipeline_info.pViewportState = &vk_viewport_state;
    vk_pipeline_info.pRasterizationState = &vk_rasterizer;
    vk_pipeline_info.pMultisampleState = &vk_multisampling;
    vk_pipeline_info.pDepthStencilState = &vk_depth_stencil;
    vk_pipeline_info.pColorBlendState = &vk_color_blending;
    vk_pipeline_info.pDynamicState = &vk_dynamic_state;
    vk_pipeline_info.layout = utils::assert_and_get_data<internal::pipeline_layout_t>(config.handle_pipeline_layout, _pipeline_layouts);
    vk_pipeline_info.renderPass = VK_NULL_HANDLE;
    vk_pipeline_info.subpass = 0;
    vk_pipeline_info.pNext = &vk_pipeline_rendering_create;

    VkResult vk_result = vkCreateGraphicsPipelines(_vkb_device, VK_NULL_HANDLE, 1, &vk_pipeline_info, nullptr, &pipeline.vk_pipeline);
    check(vk_result == VK_SUCCESS, "Failed to create graphics pipeline");

    handle_pipeline_t handle = utils::create_and_insert_new_handle<handle_pipeline_t>(_pipelines, pipeline);
    horizon_trace("created graphics pipeline");
    return handle;
}

void context_t::destroy_pipeline(handle_pipeline_t handle) {
    horizon_profile();
    internal::pipeline_t& pipeline = utils::assert_and_get_data<internal::pipeline_t>(handle, _pipelines);
    vkDestroyPipeline(_vkb_device, pipeline, nullptr);
    _pipelines.erase(handle);
}

handle_fence_t context_t::create_fence(const config_fence_t& config) {
    horizon_profile();
    internal::fence_t fence{ .config = config };
    VkFenceCreateInfo vk_fence_create_info{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    vk_fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VkResult vk_result = vkCreateFence(_vkb_device, &vk_fence_create_info, nullptr, &fence.vk_fence);
    check(vk_result == VK_SUCCESS, "Failed to create fence");
    handle_fence_t handle = utils::create_and_insert_new_handle<handle_fence_t>(_fences, fence);
    horizon_trace("created fence");
    return handle;
}

void context_t::destroy_fence(handle_fence_t handle) {
    horizon_profile();
    internal::fence_t& fence = utils::assert_and_get_data<internal::fence_t>(handle, _fences);
    vkDestroyFence(_vkb_device, fence, nullptr);
    _fences.erase(handle);
}

void context_t::wait_fence(handle_fence_t handle) {
    horizon_profile();
    internal::fence_t& fence = utils::assert_and_get_data<internal::fence_t>(handle, _fences);
    VkResult vk_result = vkWaitForFences(_vkb_device, 1, &fence.vk_fence, true, UINT64_MAX);
    check(vk_result == VK_SUCCESS, "Failed to wait for fence");
}

void context_t::reset_fence(handle_fence_t handle) {
    horizon_profile();
    internal::fence_t& fence = utils::assert_and_get_data<internal::fence_t>(handle, _fences);
    VkResult vk_result = vkResetFences(_vkb_device, 1, &fence.vk_fence);
    check(vk_result == VK_SUCCESS, "Failed to reset fence");
}

handle_semaphore_t context_t::create_semaphore(const config_semaphore_t& config) {
    horizon_profile();
    internal::semaphore_t semaphore{ .config = config };
    VkSemaphoreCreateInfo vk_semaphore_create_info{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkResult vk_result = vkCreateSemaphore(_vkb_device, &vk_semaphore_create_info, nullptr, &semaphore.vk_semaphore);
    check(vk_result == VK_SUCCESS, "Failed to create semaphore");
    handle_semaphore_t handle = utils::create_and_insert_new_handle<handle_semaphore_t>(_semaphores, semaphore);
    horizon_trace("created semaphore");
    return handle;
}

void context_t::destroy_semaphore(handle_semaphore_t handle) {
    horizon_profile();
    internal::semaphore_t& semaphore = utils::assert_and_get_data<internal::semaphore_t>(handle, _semaphores);
    vkDestroySemaphore(_vkb_device, semaphore, nullptr);
    _semaphores.erase(handle);
}

handle_command_pool_t context_t::create_command_pool(const config_command_pool_t& config) {
    horizon_profile();
    internal::command_pool_t command_pool{ .config = config };
    VkCommandPoolCreateInfo vk_command_pool_create_info{ .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    vk_command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vk_command_pool_create_info.queueFamilyIndex = _graphics_queue.vk_index;
    VkResult vk_result = vkCreateCommandPool(_vkb_device, &vk_command_pool_create_info, nullptr, &command_pool.vk_command_pool);
    check(vk_result == VK_SUCCESS, "Failed to create command pool");
    handle_command_pool_t handle = utils::create_and_insert_new_handle<handle_command_pool_t>(_command_pools, command_pool);
    horizon_trace("created command pool");
    return handle;
}

void context_t::destroy_command_pool(handle_command_pool_t handle) {
    horizon_profile();
    internal::command_pool_t& command_pool = utils::assert_and_get_data<internal::command_pool_t>(handle, _command_pools);
    vkDestroyCommandPool(_vkb_device, command_pool, nullptr);
    _command_pools.erase(handle);
}

handle_commandbuffer_t context_t::allocate_commandbuffer(const config_commandbuffer_t& config) {
    horizon_profile();
    internal::commandbuffer_t commandbuffer{ .config = config };
    VkCommandBufferAllocateInfo vk_commandbuffer_allocate_info{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    vk_commandbuffer_allocate_info.commandBufferCount = 1;
    vk_commandbuffer_allocate_info.commandPool = utils::assert_and_get_data<internal::command_pool_t>(config.handle_command_pool, _command_pools);
    vk_commandbuffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    VkResult vk_result = vkAllocateCommandBuffers(_vkb_device, &vk_commandbuffer_allocate_info, &commandbuffer.vk_commandbuffer);
    check(vk_result == VK_SUCCESS, "Failed to allocate commandbuffer");
    handle_commandbuffer_t handle = utils::create_and_insert_new_handle<handle_commandbuffer_t>(_commandbuffers, commandbuffer);
    horizon_trace("allocated commandbuffer");
    return handle;
}

void context_t::free_commandbuffer(handle_commandbuffer_t handle) {
    horizon_profile();
    internal::commandbuffer_t& commandbuffer = utils::assert_and_get_data<internal::commandbuffer_t>(handle, _commandbuffers);
    vkFreeCommandBuffers(_vkb_device, utils::assert_and_get_data<internal::command_pool_t>(commandbuffer.config.handle_command_pool, _command_pools), 1, &commandbuffer.vk_commandbuffer);
    _commandbuffers.erase(handle);
}

void context_t::begin_commandbuffer(handle_commandbuffer_t handle, bool single_use) {
    horizon_profile();
    internal::commandbuffer_t& commandbuffer = utils::assert_and_get_data<internal::commandbuffer_t>(handle, _commandbuffers);
    VkCommandBufferBeginInfo vk_commandbuffer_begin_info{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vk_commandbuffer_begin_info.flags = single_use ? VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT : 0;
    VkResult vk_result = vkBeginCommandBuffer(commandbuffer, &vk_commandbuffer_begin_info);
    check(vk_result == VK_SUCCESS, "Failed to begin commandbuffer");
}

void context_t::end_commandbuffer(handle_commandbuffer_t handle) {
    horizon_profile();
    internal::commandbuffer_t& commandbuffer = utils::assert_and_get_data<internal::commandbuffer_t>(handle, _commandbuffers);
    VkResult vk_result = vkEndCommandBuffer(commandbuffer);
    check(vk_result == VK_SUCCESS, "Failed to end commandbuffer");
}

void context_t::submit_commandbuffer(handle_commandbuffer_t handle, const std::vector<handle_semaphore_t>& wait_semaphore_handles, const std::vector<VkPipelineStageFlags>& vk_pipeline_stages, const std::vector<handle_semaphore_t>& signal_semaphore_handles, handle_fence_t handle_fence) {
    horizon_profile();
    internal::commandbuffer_t& commandbuffer = utils::assert_and_get_data<internal::commandbuffer_t>(handle, _commandbuffers);
    VkSemaphore *p_vk_wait_semaphores = reinterpret_cast<VkSemaphore *>(alloca(wait_semaphore_handles.size() * sizeof(VkSemaphore)));
    VkSemaphore *p_vk_signal_semaphores = reinterpret_cast<VkSemaphore *>(alloca(signal_semaphore_handles.size() * sizeof(VkSemaphore)));
    for (size_t i = 0; i < wait_semaphore_handles.size(); i++) {
        p_vk_wait_semaphores[i] = utils::assert_and_get_data<internal::semaphore_t>(wait_semaphore_handles[i], _semaphores);
    }
    for (size_t i = 0; i < signal_semaphore_handles.size(); i++) {
        p_vk_signal_semaphores[i] = utils::assert_and_get_data<internal::semaphore_t>(signal_semaphore_handles[i], _semaphores);
    }
    internal::fence_t& fence = utils::assert_and_get_data<internal::fence_t>(handle_fence, _fences);
    assert(vk_pipeline_stages.size() == wait_semaphore_handles.size());

    VkSubmitInfo vk_submit_info{ .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO };
    vk_submit_info.waitSemaphoreCount = wait_semaphore_handles.size();
    vk_submit_info.pWaitSemaphores = p_vk_wait_semaphores;
    vk_submit_info.signalSemaphoreCount = signal_semaphore_handles.size();
    vk_submit_info.pSignalSemaphores = p_vk_signal_semaphores;
    vk_submit_info.pWaitDstStageMask = vk_pipeline_stages.data();
    vk_submit_info.commandBufferCount = 1;
    vk_submit_info.pCommandBuffers = &commandbuffer.vk_commandbuffer;

    VkResult vk_result = vkQueueSubmit(_graphics_queue.vk_queue, 1, &vk_submit_info, fence);
    check(vk_result == VK_SUCCESS, "Failed to submit commandbuffer");
}

void context_t::cmd_bind_pipeliine(handle_commandbuffer_t handle_commandbuffer, handle_pipeline_t handle_pipeline) {
    horizon_profile();
    internal::commandbuffer_t& commandbuffer = utils::assert_and_get_data<internal::commandbuffer_t>(handle_commandbuffer, _commandbuffers);
    internal::pipeline_t& pipeline = utils::assert_and_get_data<internal::pipeline_t>(handle_pipeline, _pipelines);
    vkCmdBindPipeline(commandbuffer, pipeline.vk_pipeline_bind_point, pipeline);
}

void context_t::cmd_bind_descriptor_sets(handle_commandbuffer_t handle_commandbuffer, handle_pipeline_t handle_pipeline, uint32_t vk_first_set, const std::vector<handle_descriptor_set_t>& handle_descriptor_sets) {
    horizon_profile();
    internal::commandbuffer_t& commandbuffer = utils::assert_and_get_data<internal::commandbuffer_t>(handle_commandbuffer, _commandbuffers);
    internal::pipeline_t& pipeline = utils::assert_and_get_data<internal::pipeline_t>(handle_pipeline, _pipelines);
    internal::pipeline_layout_t& pipeline_layout = utils::assert_and_get_data<internal::pipeline_layout_t>(pipeline.config.handle_pipeline_layout, _pipeline_layouts);
    VkDescriptorSet *vk_descriptor_sets = reinterpret_cast<VkDescriptorSet *>(alloca(handle_descriptor_sets.size() * sizeof(VkDescriptorSet)));
    for (size_t i = 0; i < handle_descriptor_sets.size(); i++) {
        vk_descriptor_sets[i] = utils::assert_and_get_data<internal::descriptor_set_t>(handle_descriptor_sets[i], _descriptor_sets);
    }
    vkCmdBindDescriptorSets(commandbuffer, pipeline.vk_pipeline_bind_point, pipeline_layout, vk_first_set, handle_descriptor_sets.size(), vk_descriptor_sets, 0, nullptr);
}

void context_t::cmd_push_constants(handle_commandbuffer_t handle_commandbuffer, handle_pipeline_layout_t handle_pipeline_layout, VkShaderStageFlags vk_shader_stages, uint32_t vk_offset, uint32_t vk_size, const void *vk_data) {
    horizon_profile();
    internal::commandbuffer_t& commandbuffer = utils::assert_and_get_data<internal::commandbuffer_t>(handle_commandbuffer, _commandbuffers);
    internal::pipeline_layout_t& pipeline_layout = utils::assert_and_get_data<internal::pipeline_layout_t>(handle_pipeline_layout, _pipeline_layouts);
    vkCmdPushConstants(commandbuffer, pipeline_layout, vk_shader_stages, vk_offset, vk_size, vk_data);
}

void context_t::cmd_dispatch(handle_commandbuffer_t handle_commandbuffer, uint32_t vk_group_count_x, uint32_t vk_group_count_y, uint32_t vk_group_count_z) {
    horizon_profile();
    internal::commandbuffer_t& commandbuffer = utils::assert_and_get_data<internal::commandbuffer_t>(handle_commandbuffer, _commandbuffers);
    vkCmdDispatch(commandbuffer, vk_group_count_x, vk_group_count_y, vk_group_count_z);
}

void context_t::cmd_set_viewport_and_scissor(handle_commandbuffer_t handle_commandbuffer, VkViewport vk_viewport, VkRect2D vk_scissor) {
    horizon_profile();
    internal::commandbuffer_t& commandbuffer = utils::assert_and_get_data<internal::commandbuffer_t>(handle_commandbuffer, _commandbuffers);
    vkCmdSetViewport(commandbuffer, 0, 1, &vk_viewport);
    vkCmdSetScissor(commandbuffer, 0, 1, &vk_scissor);
}

void context_t::cmd_begin_rendering(handle_commandbuffer_t handle_commandbuffer, const std::vector<rendering_attachment_t>& color_rendering_attachments, const std::optional<rendering_attachment_t>& depth_rendering_attachment, const VkRect2D& vk_render_area, uint32_t vk_layer_count) {
    horizon_profile();
    internal::commandbuffer_t& commandbuffer = utils::assert_and_get_data<internal::commandbuffer_t>(handle_commandbuffer, _commandbuffers);
    VkRenderingInfo vk_rendering_info{ .sType = VK_STRUCTURE_TYPE_RENDERING_INFO };
    VkRenderingAttachmentInfo *p_vk_color_attachment_infos = reinterpret_cast<VkRenderingAttachmentInfo *>(alloca(color_rendering_attachments.size() * sizeof(VkRenderingAttachmentInfo)));
    for (size_t i = 0; i < color_rendering_attachments.size(); i++) {
        p_vk_color_attachment_infos[i] = { .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        p_vk_color_attachment_infos[i].clearValue = color_rendering_attachments[i].clear_value;
        p_vk_color_attachment_infos[i].imageLayout = color_rendering_attachments[i].image_layout;
        p_vk_color_attachment_infos[i].loadOp = color_rendering_attachments[i].load_op;
        p_vk_color_attachment_infos[i].storeOp = color_rendering_attachments[i].store_op;
        p_vk_color_attachment_infos[i].imageView = utils::assert_and_get_data<internal::image_view_t>(color_rendering_attachments[i].handle_image_view, _image_views);
    }
    VkRenderingAttachmentInfo vk_depth_rendering_attachment{ .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    if (depth_rendering_attachment.has_value()) {
        vk_depth_rendering_attachment.clearValue = depth_rendering_attachment.value().clear_value;
        vk_depth_rendering_attachment.imageLayout = depth_rendering_attachment.value().image_layout;
        vk_depth_rendering_attachment.loadOp = depth_rendering_attachment.value().load_op;
        vk_depth_rendering_attachment.storeOp = depth_rendering_attachment.value().store_op;
        vk_depth_rendering_attachment.imageView = utils::assert_and_get_data<internal::image_view_t>(depth_rendering_attachment.value().handle_image_view, _image_views);
        vk_rendering_info.pDepthAttachment = &vk_depth_rendering_attachment;
    }
    vk_rendering_info.renderArea = vk_render_area;
    vk_rendering_info.layerCount = vk_layer_count;
    vk_rendering_info.colorAttachmentCount = color_rendering_attachments.size();
    vk_rendering_info.pColorAttachments = p_vk_color_attachment_infos;
    vkCmdBeginRendering(commandbuffer, &vk_rendering_info);
}

void context_t::cmd_end_rendering(handle_commandbuffer_t handle_commandbuffer) {
    horizon_profile();
    internal::commandbuffer_t& commandbuffer = utils::assert_and_get_data<internal::commandbuffer_t>(handle_commandbuffer, _commandbuffers);
    vkCmdEndRendering(commandbuffer);
}

void context_t::cmd_draw(handle_commandbuffer_t handle_commandbuffer, uint32_t vk_vertex_count, uint32_t vk_instance_count, uint32_t vk_first_vertex, uint32_t vk_first_instance) {
    horizon_profile();
    internal::commandbuffer_t& commandbuffer = utils::assert_and_get_data<internal::commandbuffer_t>(handle_commandbuffer, _commandbuffers);
    vkCmdDraw(commandbuffer, vk_vertex_count, vk_instance_count, vk_first_vertex, vk_first_instance);
}

void context_t::cmd_image_memory_barrier(handle_commandbuffer_t handle_commandbuffer, handle_image_t handle_image, VkImageLayout vk_old_image_layout, VkImageLayout vk_new_image_layout, VkAccessFlags vk_src_access_mask, VkAccessFlags vk_dst_access_mask, VkPipelineStageFlags vk_src_pipeline_stage, VkPipelineStageFlags vk_dst_pipeline_stage) {
    horizon_profile();
    internal::commandbuffer_t& commandbuffer = utils::assert_and_get_data<internal::commandbuffer_t>(handle_commandbuffer, _commandbuffers);
    internal::image_t& image = utils::assert_and_get_data<internal::image_t>(handle_image, _images);
    VkImageMemoryBarrier vk_image_memory_barrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    vk_image_memory_barrier.oldLayout = vk_old_image_layout;
    vk_image_memory_barrier.newLayout = vk_new_image_layout;
    vk_image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vk_image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vk_image_memory_barrier.image = image;
    vk_image_memory_barrier.subresourceRange.aspectMask = utils::get_image_aspect(image.config.vk_format);
    vk_image_memory_barrier.subresourceRange.baseMipLevel = 0;
    vk_image_memory_barrier.subresourceRange.levelCount = 1;
    vk_image_memory_barrier.subresourceRange.baseArrayLayer = 0;
    vk_image_memory_barrier.subresourceRange.layerCount = 1;
    vk_image_memory_barrier.srcAccessMask = vk_src_access_mask;
    vk_image_memory_barrier.dstAccessMask = vk_dst_access_mask;
    vkCmdPipelineBarrier(commandbuffer, vk_src_pipeline_stage, vk_dst_pipeline_stage, 0, 0, nullptr, 0, nullptr, 1, &vk_image_memory_barrier);
}

} // namespace gfx

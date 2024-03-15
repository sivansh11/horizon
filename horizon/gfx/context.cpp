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

namespace gfx {

static VkImageAspectFlags get_image_aspect(VkFormat format) {
    horizon_profile();
    static std::set<VkFormat> vk_depth_formats = {
        VK_FORMAT_D16_UNORM,
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
    };

    if (vk_depth_formats.contains(format)) 
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    return VK_IMAGE_ASPECT_COLOR_BIT;
}

descriptor_set_layout_config_t& descriptor_set_layout_config_t::add_layout_binding(const VkDescriptorSetLayoutBinding& vk_descriptor_set_layout_binding) {
    horizon_profile();
    vk_descriptor_set_layout_bindings.push_back(vk_descriptor_set_layout_binding);
    return *this;
}

descriptor_set_layout_config_t& descriptor_set_layout_config_t::add_layout_binding(uint32_t binding, VkDescriptorType descriptor_type, VkShaderStageFlags stage_flags, uint32_t count) {
    horizon_profile();
    VkDescriptorSetLayoutBinding vk_descriptor_set_layout_binding{};
    vk_descriptor_set_layout_binding.binding = binding;
    vk_descriptor_set_layout_binding.descriptorType = descriptor_type;
    vk_descriptor_set_layout_binding.descriptorCount = count;
    vk_descriptor_set_layout_binding.stageFlags = stage_flags;
    vk_descriptor_set_layout_bindings.push_back(vk_descriptor_set_layout_binding);
    return *this;
}

pipeline_config_t::pipeline_config_t() {
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

pipeline_config_t& pipeline_config_t::add_shader(shader_module_handle_t handle) {
    horizon_profile();
    shaders.push_back(handle);
    return *this;
}

pipeline_config_t& pipeline_config_t::add_descriptor_set_layout(descriptor_set_layout_handle_t handle) {
    horizon_profile();
    descriptor_set_layouts.push_back(handle);
    return *this;
}

pipeline_config_t& pipeline_config_t::add_push_constant(uint32_t vk_size, uint32_t vk_offset, VkShaderStageFlags vk_shader_stage) {
    horizon_profile();
    vk_push_constant_ranges.push_back(VkPushConstantRange{ .stageFlags = vk_shader_stage, .offset = vk_offset, .size = vk_size });
    return *this;
}

pipeline_config_t& pipeline_config_t::add_color_attachment(VkFormat vk_format, const VkPipelineColorBlendAttachmentState& vk_pipeline_color_blend_state) {
    horizon_profile();
    vk_color_formats.push_back(vk_format);
    vk_pipeline_color_blend_attachment_states.push_back(vk_pipeline_color_blend_state);
    return *this;
}

pipeline_config_t& pipeline_config_t::set_depth(VkFormat vk_format, const VkPipelineDepthStencilStateCreateInfo& vk_pipeline_color_blend_state) {
    horizon_profile();
    vk_depth_format = vk_format;
    this->vk_pipeline_depth_stencil_state_create_info = vk_pipeline_color_blend_state;
    return *this;
}

pipeline_config_t& pipeline_config_t::add_dynamic_state(const VkDynamicState& vk_dynamic_state) {
    horizon_profile();
    vk_dynamic_states.push_back(vk_dynamic_state);
    return *this;
}

pipeline_config_t& pipeline_config_t::add_vertex_input_binding_description(uint32_t vk_binding, uint32_t vk_stride, VkVertexInputRate vk_input_rate) {
    horizon_profile();
    VkVertexInputBindingDescription vk_vertex_input_binding_description{};
    vk_vertex_input_binding_description.binding = vk_binding;
    vk_vertex_input_binding_description.stride = vk_stride;
    vk_vertex_input_binding_description.inputRate = vk_input_rate;
    vk_vertex_input_binding_descriptions.push_back(vk_vertex_input_binding_description);
    return *this;
}

pipeline_config_t& pipeline_config_t::add_vertex_input_attribute_description(uint32_t vk_binding, uint32_t vk_location, VkFormat vk_format, uint32_t vk_offset) {
    horizon_profile();
    VkVertexInputAttributeDescription vk_vertex_input_attribute_description{};
    vk_vertex_input_attribute_description.binding = vk_binding;
    vk_vertex_input_attribute_description.format = vk_format;
    vk_vertex_input_attribute_description.location = vk_location;
    vk_vertex_input_attribute_description.offset = vk_offset;
    vk_vertex_input_attribute_descriptions.push_back(vk_vertex_input_attribute_description);
    return *this;
}

pipeline_config_t& pipeline_config_t::set_pipeline_input_assembly_state(const VkPipelineInputAssemblyStateCreateInfo& vk_pipeline_input_assembly_state) {
    horizon_profile();
    this->vk_pipeline_input_assembly_state = vk_pipeline_input_assembly_state;
    return *this;
}

pipeline_config_t& pipeline_config_t::set_pipeline_rasterization_state(const VkPipelineRasterizationStateCreateInfo& vk_pipeline_rasterization_state) {
    horizon_profile();
    this->vk_pipeline_rasterization_state = vk_pipeline_rasterization_state;
    return *this;
}

pipeline_config_t& pipeline_config_t::set_pipeline_multisample_state(const VkPipelineMultisampleStateCreateInfo& vk_pipeline_multisample_state) {
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

void command_list_t::bind_pipeline(pipeline_handle_t pipeline_handle) {
    horizon_profile();
    command_t command{ .type = command_type_t::e_bind_pipeline };
    command.as.bind_pipeline.handle = pipeline_handle;
    _commands.push_back(command);
}

void command_list_t::dispatch(uint32_t x, uint32_t y, uint32_t z) {
    horizon_profile();
    command_t command{ .type = command_type_t::e_dispatch };
    command.as.dispatch = { x, y, z };
    _commands.push_back(command);
}

void command_list_t::begin_rendering(const command_begin_rendering_t& command_begin_rendering) {
    horizon_profile();
    command_t command{ .type = command_type_t::e_begin_rendering };
    command.as.begin_rendering = command_begin_rendering;
    _commands.push_back(command);
}

void command_list_t::end_rendering() {
    horizon_profile();
    command_t command{ .type = command_type_t::e_end_rendering };
    command.as.end_rendering = {};
    _commands.push_back(command);
}

void command_list_t::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) {
    horizon_profile();
    command_t command{ .type = command_type_t::e_draw };
    command.as.draw.vertex_count = vertex_count;
    command.as.draw.instance_count = instance_count;
    command.as.draw.first_vertex = first_vertex;
    command.as.draw.first_instance = first_instance;
    _commands.push_back(command);
}

void command_list_t::image_memory_barrier(image_handle_t handle, VkImageLayout old_image_layout, VkImageLayout new_image_layout, VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask, VkPipelineStageFlags src_pipeline_stage, VkPipelineStageFlags dst_pipeline_stage) {
    horizon_profile();
    command_t command{ .type = command_type_t::e_image_memory_barrier };
    command.as.image_memory_barrier.image = handle;
    command.as.image_memory_barrier.old_image_layout = old_image_layout;
    command.as.image_memory_barrier.new_image_layout = new_image_layout;
    command.as.image_memory_barrier.src_access_mask = src_access_mask;
    command.as.image_memory_barrier.dst_access_mask = dst_access_mask;
    command.as.image_memory_barrier.src_pipeline_stage = src_pipeline_stage;
    command.as.image_memory_barrier.dst_pipeline_stage = dst_pipeline_stage;
    _commands.push_back(command);
}

void command_list_t::set_viewport_and_scissor(const VkViewport& viewport, const VkRect2D& scissor) {
    horizon_profile();
    command_t command{ .type = command_type_t::e_set_viewport_and_scissor };
    command.as.set_viewport_and_scissor.viewport = viewport;
    command.as.set_viewport_and_scissor.scissor = scissor;
    _commands.push_back(command);
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

context_t::context_t(bool validation) : _validation(validation) {
    horizon_profile();
    create_instance();
    create_device();
    create_vma();
    // create_command_pool();
    create_descriptor_pool();
}

context_t::~context_t() {
    horizon_profile();

    vkDeviceWaitIdle(_vk_device);
    for (auto& [command_pool_handle, command_pool] : _command_pools) {
        vkDestroyCommandPool(_vk_device, command_pool, nullptr);
    }
    for (auto& [fence_handle, fence] : _fences) {
        wait_fence<1>({fence_handle});
        vkDestroyFence(_vk_device, fence, nullptr);
    }
    for (auto& [semaphore_handle, semaphore] : _semaphores) {
        vkDestroySemaphore(_vk_device, semaphore, nullptr);
    }
    for (auto& [image_view_handle, image_view] : _image_views) {
        vkDestroyImageView(_vk_device, image_view, nullptr);
    }
    for (auto& [image_handle, image] : _images) if (!image.from_swapchain) { 
        if (image.map) unmap_image(image_handle);
        vmaDestroyImage(_vma_allocator, image, image.vma_allocation);
    }
    for (auto& [buffer_handle, buffer] : _buffers) {
        if (buffer.map) unmap_buffer(buffer_handle);
        vmaDestroyBuffer(_vma_allocator, buffer, buffer.vma_allocation);
    }
    for (auto& [handle, descriptor_set_layout] : _descriptor_set_layouts) {
        vkDestroyDescriptorSetLayout(_vk_device, descriptor_set_layout, nullptr);
    }
    for (auto& [pipeline_handle, pipeline] : _pipelines) {
        vkDestroyPipelineLayout(_vk_device, pipeline.vk_layout, nullptr);
        vkDestroyPipeline(_vk_device, pipeline, nullptr);
    }
    for (auto& [shader_module_handle, shader_module] : _shader_modules) {
        vkDestroyShaderModule(_vk_device, shader_module.vk_module, nullptr);
    }
    vkDestroyDescriptorPool(_vk_device, _vk_descriptor_pool, nullptr);
    for (auto& [swapchain_handle, swapchain] : _swapchains) {
        vkb::destroy_swapchain(swapchain.vk_swapchain);
        vkDestroySurfaceKHR(_vk_instance, swapchain.vk_surface, nullptr);
    }
    vmaDestroyAllocator(_vma_allocator);
    vkb::destroy_device(_vk_device);
    vkb::destroy_instance(_vk_instance);
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
    }
    return VK_FALSE;
}

void context_t::create_instance() {
    horizon_profile();

    vkb::InstanceBuilder vk_instance_builder{};
    vk_instance_builder.set_debug_callback(debug_callback)
                    .desire_api_version(VK_API_VERSION_1_3)
                    .enable_extension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)
                    .request_validation_layers(_validation);
    auto result = vk_instance_builder.build();
    if (!result) {
        horizon_error("Failed to create instance!");
        std::terminate();
    }
    _vk_instance = result.value();
    volkLoadInstance(_vk_instance);

    horizon_trace("created instance");
}

void context_t::create_device() {
    horizon_profile();
    vkb::PhysicalDeviceSelector vk_physical_device_selector{_vk_instance};
    vk_physical_device_selector.add_required_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    vk_physical_device_selector.add_required_extension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    vk_physical_device_selector.add_required_extension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
    vk_physical_device_selector.add_required_extension(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
    vk_physical_device_selector.add_required_extension(VK_KHR_BIND_MEMORY_2_EXTENSION_NAME);
    vk_physical_device_selector.add_required_extension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    VkPhysicalDeviceScalarBlockLayoutFeatures vk_physical_device_scalar_block_layout_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES,
        .scalarBlockLayout = VK_TRUE,
    };
    vk_physical_device_selector.add_required_extension_features<VkPhysicalDeviceScalarBlockLayoutFeatures>(vk_physical_device_scalar_block_layout_features);
    VkPhysicalDeviceBufferDeviceAddressFeatures vk_physical_device_buffer_device_address_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
        .bufferDeviceAddress = VK_TRUE,
        .bufferDeviceAddressCaptureReplay = VK_FALSE,
        .bufferDeviceAddressMultiDevice = VK_FALSE
    };
    vk_physical_device_selector.add_required_extension_features<VkPhysicalDeviceBufferDeviceAddressFeatures>(vk_physical_device_buffer_device_address_features);
    VkPhysicalDeviceDynamicRenderingFeaturesKHR vk_physical_device_dynamic_rendering_features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES };
    vk_physical_device_dynamic_rendering_features.dynamicRendering = VK_TRUE;
    vk_physical_device_selector.add_required_extension_features<VkPhysicalDeviceDynamicRenderingFeaturesKHR>(vk_physical_device_dynamic_rendering_features);
    vk_physical_device_selector.prefer_gpu_device_type(vkb::PreferredDeviceType::integrated);

    // create temp window, to get surface
    core::window_t temp_window{"temp", 2, 2};

    VkSurfaceKHR vk_temp_surface;
    {
        auto result = glfwCreateWindowSurface(_vk_instance, temp_window.window(), nullptr, &vk_temp_surface);
        if (result != VK_SUCCESS) {
            horizon_error("Failed to create surface");
            std::terminate();
        }
    }

    vk_physical_device_selector.set_surface(vk_temp_surface);

    {
        auto result = vk_physical_device_selector.select(vkb::DeviceSelectionMode::only_fully_suitable);
        if (!result) {
            horizon_error("Failed to pick suitable physical device");
            std::terminate();
        }
        _vk_physical_device = result.value();
    }

    vkb::DeviceBuilder vk_device_builder{ _vk_physical_device };
    {
        auto result = vk_device_builder.build();
        if (!result) {
            horizon_error("Failed to create device!");
            std::terminate();
        }
        _vk_device = result.value();
    }
    { 
        auto result =_vk_device.get_queue(vkb::QueueType::graphics);
        if (!result) {
            horizon_error("Failed to get graphics queue");
            std::terminate();
        }
        _vk_graphics_queue.vk_queue = result.value();
    }
    {
        auto result = _vk_device.get_queue_index(vkb::QueueType::graphics);
        if (!result) {
            horizon_error("Failed to get graphics queue index");
            std::terminate();
        }
        _vk_graphics_queue.vk_index = result.value();
    }
    { 
        auto result =_vk_device.get_queue(vkb::QueueType::present);
        if (!result) {
            horizon_error("Failed to get present queue");
            std::terminate();
        }
        _vk_present_queue.vk_queue = result.value();
    }
    {
        auto result = _vk_device.get_queue_index(vkb::QueueType::present);
        if (!result) {
            horizon_error("Failed to get present queue index");
            std::terminate();
        }
        _vk_present_queue.vk_index = result.value();
    }

    vkDestroySurfaceKHR(_vk_instance, vk_temp_surface, nullptr);
    
    volkLoadDevice(_vk_device);

    horizon_trace("created device");
}

void context_t::create_vma() {
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
        .physicalDevice = _vk_physical_device,
        .device = _vk_device,
        .pVulkanFunctions = &vma_vulkan_functions,
        .instance = _vk_instance,
        .vulkanApiVersion = VK_API_VERSION_1_3,
    };

    auto result = vmaCreateAllocator(&vma_allocator_create_info, &_vma_allocator);
    if (result != VK_SUCCESS) {
        horizon_error("Failed to create vma allocator");
        std::terminate();
    }
    horizon_trace("create vma allocator");
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

    if (vkCreateDescriptorPool(_vk_device, &vk_descriptor_pool_create_info, nullptr, &_vk_descriptor_pool) != VK_SUCCESS) {
        horizon_error("Failed to create descriptor pool");
        std::terminate();
    }
    horizon_trace("created descriptor pool");
}

swapchain_handle_t context_t::create_swapchain(const core::window_t& window) {
    horizon_profile();
    swapchain_t swapchain{};
    {
        auto result = glfwCreateWindowSurface(_vk_instance, window.window(), nullptr, &swapchain.vk_surface);
        if (result != VK_SUCCESS) {
            horizon_error("Failed to create surface");
            std::terminate();
        }
    }
    {
        vkb::SwapchainBuilder vk_swapchain_builder{_vk_device, swapchain.vk_surface};
        auto result = vk_swapchain_builder.build();
        if (!result) {
            horizon_error("Failed to create swapchain");
            std::terminate();
        }
        swapchain.vk_swapchain = result.value();    
    }

    std::vector<VkImage> vk_images;
    {
        auto result = swapchain.vk_swapchain.get_images();
        if (!result) {
            horizon_error("Failed to get swapchain images");
            std::terminate();
        }
        vk_images = result.value();
    }

    for (auto vk_image : vk_images) {
        image_config_t config{};
        config.vk_array_layers = 1;
        config.vk_depth = 1;
        config.vk_format = swapchain.vk_swapchain.image_format;
        config.vk_width = swapchain.vk_swapchain.extent.width;
        config.vk_height = swapchain.vk_swapchain.extent.height;
        config.vk_inital_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        config.vk_mips = 1;
        config.vk_sample_count = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
        config.vk_type = VK_IMAGE_TYPE_2D;
        image_t image{ .config = config };
        image.vk_image = vk_image;
        image.from_swapchain = true;

        image_handle_t image_handle = _images.size();
        while (_images.contains(image_handle)) {
            image_handle++;
        }

        _images.insert({image_handle, image});

        auto image_view_handle = create_image_view(image_handle, {});

        swapchain.images.push_back(image_handle);
        swapchain.image_views.push_back(image_view_handle);
    }

    swapchain_handle_t handle = _swapchains.size();
    while (_swapchains.contains(handle)) {
        handle++;
    }

    _swapchains.insert({handle, swapchain});
    
    horizon_trace("created swapchain with handle: {}", handle);

    return handle;
}

void context_t::destroy_swapchain(swapchain_handle_t handle) {
    horizon_profile();
    assert(_swapchains.contains(handle));
    swapchain_t& swapchain = _swapchains[handle];
    vkDestroySwapchainKHR(_vk_device, swapchain.vk_swapchain, nullptr);
    vkDestroySurfaceKHR(_vk_instance, swapchain.vk_surface, nullptr);
    _swapchains.erase(handle);  
}

std::vector<image_handle_t> context_t::swapchain_images(swapchain_handle_t handle) {
    horizon_profile();
    assert(_swapchains.contains(handle));
    swapchain_t& swapchain = _swapchains[handle];
    return swapchain.images;
}

std::vector<image_view_handle_t> context_t::swapchain_image_views(swapchain_handle_t handle) {
    horizon_profile();
    assert(_swapchains.contains(handle));
    swapchain_t& swapchain = _swapchains[handle];
    return swapchain.image_views;
}

std::optional<uint32_t> context_t::acquire_swapchain_next_image_index(swapchain_handle_t handle, semaphore_handle_t semaphore_handle, fence_handle_t fence_handle) {
    horizon_profile();
    assert(_swapchains.contains(handle));
    swapchain_t& swapchain = _swapchains[handle];
    VkSemaphore vk_semaphore = VK_NULL_HANDLE;
    VkFence vk_fence = VK_NULL_HANDLE;
    if (semaphore_handle != null_handle) {
        assert(_semaphores.contains(semaphore_handle));
        semaphore_t& _semaphore = _semaphores[semaphore_handle];
        vk_semaphore = _semaphore;
    }
    if (fence_handle != null_handle) {
        assert(_fences.contains(fence_handle));
        fence_t& _fence = _fences[fence_handle];
        vk_fence = _fence;

    }
    uint32_t next_image;
    {
        auto result = vkAcquireNextImageKHR(_vk_device, swapchain.vk_swapchain, UINT64_MAX, vk_semaphore, vk_fence, &next_image);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            return std::nullopt;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            horizon_error("Failed to acquire swap chain image");
            std::terminate();
        }
        return next_image;
    }
}

shader_module_handle_t context_t::create_shader_module(const shader_module_config_t& config) {
    horizon_profile();
    shaderc_shader_kind shaderc_kind;

    if (config.type == shader_type_t::e_vertex) shaderc_kind = shaderc_vertex_shader;
    if (config.type == shader_type_t::e_fragment) shaderc_kind = shaderc_fragment_shader;
    if (config.type == shader_type_t::e_compute) shaderc_kind = shaderc_compute_shader;
    
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

    shader_module_t shader_module{ .config = config };

    auto result = vkCreateShaderModule(_vk_device, &vk_shader_module_create_info, nullptr, &shader_module.vk_module);
    if (result != VK_SUCCESS) {
        horizon_error("Failed to create shader module");
        std::terminate();
    }

    shader_module_handle_t handle = _shader_modules.size();
    while (_shader_modules.contains(handle)) {
        handle++;
    }

    _shader_modules.insert({handle, shader_module});

    horizon_trace("shader module created with name: {}", config.name);
    return handle;
}

void context_t::destroy_shader_module(shader_module_handle_t handle) {
    horizon_profile();
    assert(_shader_modules.contains(handle));
    VkShaderModule vk_shader_module = _shader_modules[handle];
    vkDestroyShaderModule(_vk_device, vk_shader_module, nullptr);
    _shader_modules.erase(handle);    
}

pipeline_handle_t context_t::create_compute_pipeline(const pipeline_config_t& config) {
    horizon_profile();
    assert(config.shaders.size() == 1);  // for compute it should only be 1
    pipeline_t pipeline{ .config = config, .vk_bind_point = VK_PIPELINE_BIND_POINT_COMPUTE };

    // maybe move this to its own function
    VkDescriptorSetLayout *vk_set_layouts = reinterpret_cast<VkDescriptorSetLayout *>(alloca(config.descriptor_set_layouts.size() * sizeof(VkDescriptorSetLayout)));
    for (size_t i = 0; i < config.descriptor_set_layouts.size(); i++) {
        assert(_descriptor_set_layouts.contains(config.descriptor_set_layouts[i]));
        auto& descriptor_set_layout = _descriptor_set_layouts[config.descriptor_set_layouts[i]];
        vk_set_layouts[i] = descriptor_set_layout;
    }
    VkPipelineLayoutCreateInfo vk_pipeline_layout_create_info{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    vk_pipeline_layout_create_info.setLayoutCount = config.descriptor_set_layouts.size();
    vk_pipeline_layout_create_info.pSetLayouts = vk_set_layouts;
    vk_pipeline_layout_create_info.pushConstantRangeCount = config.vk_push_constant_ranges.size(); 
    vk_pipeline_layout_create_info.pPushConstantRanges = config.vk_push_constant_ranges.data();
    {
        auto result = vkCreatePipelineLayout(_vk_device, &vk_pipeline_layout_create_info, nullptr, &pipeline.vk_layout);
        if (result != VK_SUCCESS) {
            horizon_error("Failed to create pipeline layout");
            std::terminate();
        }
    }

    // maybe move this to its own function
    VkPipelineShaderStageCreateInfo vk_pipeline_shader_stage_create_info{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    vk_pipeline_shader_stage_create_info.pName = "main";
    vk_pipeline_shader_stage_create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    vk_pipeline_shader_stage_create_info.module = _shader_modules[config.shaders[0]];

    VkComputePipelineCreateInfo vk_compute_pipeline_create_info{ .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    vk_compute_pipeline_create_info.layout = pipeline.vk_layout;
    vk_compute_pipeline_create_info.stage = vk_pipeline_shader_stage_create_info;
    {
        auto result = vkCreateComputePipelines(_vk_device, VK_NULL_HANDLE, 1, &vk_compute_pipeline_create_info, nullptr, &pipeline.vk_pipeline);
        if (result != VK_SUCCESS) {
            horizon_error("Failed to create compute pipeline");
            std::terminate();
        }
    }
    
    pipeline_handle_t handle = _pipelines.size();
    while (_pipelines.contains(handle)) {
        handle++;
    }

    _pipelines.insert({handle, pipeline});
    horizon_trace("created pipeline");
    return handle;
}

pipeline_handle_t context_t::create_graphics_pipeline(const pipeline_config_t& config) {
    horizon_profile();
    pipeline_t pipeline{ .config = config, .vk_bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS };

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

    for (auto& shader_module_handle : config.shaders) {
        assert(_shader_modules.contains(shader_module_handle));
        auto& shader_module = _shader_modules[shader_module_handle];

        VkPipelineShaderStageCreateInfo vk_pipeline_shader_stage_create_info{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        vk_pipeline_shader_stage_create_info.pName = "main";
        switch (shader_module.config.type) {
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
        assert(_shader_modules.contains(shader_module_handle));
        vk_pipeline_shader_stage_create_info.module = _shader_modules[shader_module_handle];
        vk_pipeline_shader_stage_create_infos.push_back(vk_pipeline_shader_stage_create_info);
    }

    // maybe move this to its own function
    VkDescriptorSetLayout *vk_set_layouts = reinterpret_cast<VkDescriptorSetLayout *>(alloca(config.descriptor_set_layouts.size() * sizeof(VkDescriptorSetLayout)));
    for (size_t i = 0; i < config.descriptor_set_layouts.size(); i++) {
        assert(_descriptor_set_layouts.contains(config.descriptor_set_layouts[i]));
        auto& descriptor_set_layout = _descriptor_set_layouts[config.descriptor_set_layouts[i]];
        vk_set_layouts[i] = descriptor_set_layout;
    }
    VkPipelineLayoutCreateInfo vk_pipeline_layout_create_info{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    vk_pipeline_layout_create_info.setLayoutCount = config.descriptor_set_layouts.size();
    vk_pipeline_layout_create_info.pSetLayouts = vk_set_layouts;
    vk_pipeline_layout_create_info.pushConstantRangeCount = config.vk_push_constant_ranges.size(); 
    vk_pipeline_layout_create_info.pPushConstantRanges = config.vk_push_constant_ranges.data();
    {
        auto result = vkCreatePipelineLayout(_vk_device, &vk_pipeline_layout_create_info, nullptr, &pipeline.vk_layout);
        if (result != VK_SUCCESS) {
            horizon_error("Failed to create pipeline layout");
            std::terminate();
        }
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

    VkPipelineRenderingCreateInfoKHR vk_pipeline_rendering_create{ .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR };
    vk_pipeline_rendering_create.pNext                   = nullptr;
    vk_pipeline_rendering_create.colorAttachmentCount    = config.vk_color_formats.size();
    vk_pipeline_rendering_create.pColorAttachmentFormats = config.vk_color_formats.data();
    vk_pipeline_rendering_create.depthAttachmentFormat   = config.vk_depth_format;
    vk_pipeline_rendering_create.stencilAttachmentFormat = config.vk_depth_format;

    VkGraphicsPipelineCreateInfo vk_pipeline_info{};
    vk_pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    vk_pipeline_info.stageCount = static_cast<uint32_t>(config.shaders.size());
    vk_pipeline_info.pStages = vk_pipeline_shader_stage_create_infos.data();
    vk_pipeline_info.pVertexInputState = &vk_vertex_input_info;
    vk_pipeline_info.pInputAssemblyState = &vk_input_assembly;
    vk_pipeline_info.pViewportState = &vk_viewport_state;
    vk_pipeline_info.pRasterizationState = &vk_rasterizer;
    vk_pipeline_info.pMultisampleState = &vk_multisampling;
    vk_pipeline_info.pDepthStencilState = &vk_depth_stencil;
    vk_pipeline_info.pColorBlendState = &vk_color_blending;
    vk_pipeline_info.pDynamicState = &vk_dynamic_state;
    vk_pipeline_info.layout = pipeline.vk_layout;
    vk_pipeline_info.renderPass = VK_NULL_HANDLE;
    vk_pipeline_info.subpass = 0;
    vk_pipeline_info.pNext = &vk_pipeline_rendering_create;

    auto result = vkCreateGraphicsPipelines(_vk_device, VK_NULL_HANDLE, 1, &vk_pipeline_info, nullptr, &pipeline.vk_pipeline);
    if (result != VK_SUCCESS) {
        horizon_error("Failed to create graphics pipeline");
        std::terminate();
    }
    
    pipeline_handle_t handle = _pipelines.size();
    while (_pipelines.contains(handle)) {
        handle++;
    }

    _pipelines.insert({handle, pipeline});
    horizon_trace("created pipeline");
    return handle;
}

void context_t::destroy_pipeline(pipeline_handle_t handle) {
    horizon_profile();
    assert(_pipelines.contains(handle));
    pipeline_t& pipeline = _pipelines[handle];
    vkDestroyPipelineLayout(_vk_device, pipeline.vk_layout, nullptr);
    vkDestroyPipeline(_vk_device, pipeline, nullptr);
    _pipelines.erase(handle);    
}

buffer_handle_t context_t::create_buffer(const buffer_config_t& config) {
    horizon_profile();
    VkBufferCreateInfo vk_buffer_create_info{ .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    vk_buffer_create_info.size = config.size;
    vk_buffer_create_info.usage = config.vk_usage_flags;

    VmaAllocationCreateInfo vma_allocation_create_info{};
    vma_allocation_create_info.usage = config.vma_memory_usage;
    vma_allocation_create_info.flags = config.vma_allocation_create_flags;

    buffer_t buffer{ .config = config };

    auto result = vmaCreateBuffer(_vma_allocator, &vk_buffer_create_info, &vma_allocation_create_info, &buffer.vk_buffer, &buffer.vma_allocation, nullptr);
    if (result != VK_SUCCESS) {
        horizon_error("Failed to create buffer");
        std::terminate();
    }

    buffer_handle_t handle = _buffers.size();
    while (_buffers.contains(handle)) {
        handle++;
    }

    _buffers.insert({handle, buffer});
    horizon_trace("created buffer");
    return handle;
}

void context_t::destroy_buffer(buffer_handle_t handle) {
    horizon_profile();
    assert(_buffers.contains(handle));
    buffer_t& buffer = _buffers[handle];
    vmaDestroyBuffer(_vma_allocator, buffer, buffer.vma_allocation);
    _buffers.erase(handle);    
}

void *context_t::map_buffer(buffer_handle_t handle) {
    horizon_profile();
    assert(_buffers.contains(handle));
    buffer_t& buffer = _buffers[handle];
    if (buffer.map) return buffer.map;
    vmaMapMemory(_vma_allocator, buffer.vma_allocation, &buffer.map);
    return buffer.map;
}

void context_t::unmap_buffer(buffer_handle_t handle) {
    horizon_profile();
    assert(_buffers.contains(handle));
    buffer_t& buffer = _buffers[handle];
    assert(buffer.map);
    vmaUnmapMemory(_vma_allocator, buffer.vma_allocation);
}

image_handle_t context_t::create_image(const image_config_t& config) {
    horizon_profile();
    VkImageCreateInfo vk_image_create_info{ .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    vk_image_create_info.imageType = config.vk_type;
    vk_image_create_info.extent = { config.vk_width, config.vk_height, config.vk_depth };
    
    if (config.vk_mips == vk_auto_calculate_mip_levels) {
        VkImageFormatProperties image_format_properties{};
        vkGetPhysicalDeviceImageFormatProperties(_vk_physical_device, VK_FORMAT_R8G8B8A8_SRGB, VkImageType::VK_IMAGE_TYPE_2D, VkImageTiling::VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 0, &image_format_properties);
        vk_image_create_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(std::max(config.vk_width, config.vk_height), config.vk_depth)))) + 1;  // this might be wrong ?
        vk_image_create_info.mipLevels = std::min(image_format_properties.maxMipLevels, vk_image_create_info.mipLevels); 
    } else {
        vk_image_create_info.mipLevels = config.vk_mips;
    }

    vk_image_create_info.arrayLayers = config.vk_array_layers;
    vk_image_create_info.format = config.vk_format;
    vk_image_create_info.tiling = config.vk_tiling;
    vk_image_create_info.initialLayout = config.vk_inital_layout;
    vk_image_create_info.usage = config.vk_usage;
    vk_image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vk_image_create_info.samples = config.vk_sample_count;
    vk_image_create_info.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

    image_t image{ .config = config };
    image.config.vk_mips = vk_image_create_info.mipLevels;

    VmaAllocationCreateInfo vma_allocation_create_info{};
    vma_allocation_create_info.usage = config.vma_memory_usage;
    vma_allocation_create_info.flags = config.vma_allocation_create_flags;

    auto result = vmaCreateImage(_vma_allocator, &vk_image_create_info, &vma_allocation_create_info, &image.vk_image, &image.vma_allocation, nullptr);
    if (result != VK_SUCCESS) {
        horizon_error("Failed to create image");
        std::terminate();
    }

    image_handle_t handle = _images.size();
    while (_images.contains(handle)) {
        handle++;
    }

    _images.insert({handle, image});
    horizon_trace("created image");
    return handle;
}

void context_t::destroy_image(image_handle_t handle) {
    horizon_profile();
    assert(_images.contains(handle));
    image_t& image = _images[handle];
    vmaDestroyImage(_vma_allocator, image, image.vma_allocation);
    _images.erase(handle);    
}

void *context_t::map_image(image_handle_t handle) {
    horizon_profile();
    assert(_images.contains(handle));
    image_t& image = _images[handle];
    vmaMapMemory(_vma_allocator, image.vma_allocation, &image.map);
    return image.map;
}

void context_t::unmap_image(image_handle_t handle) {
    horizon_profile();
    assert(_images.contains(handle));
    image_t& image = _images[handle];
    assert(image.map);
    vmaUnmapMemory(_vma_allocator, image.vma_allocation);
}

image_view_handle_t context_t::create_image_view(image_handle_t image_handle, const image_view_config_t& config) {
    horizon_profile();
    assert(_images.contains(image_handle));
    auto& image = _images[image_handle];
    
    image_view_t image_view{ .config = config, .image_handle = image_handle };

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
    vk_image_view_create_info.subresourceRange.aspectMask = get_image_aspect(image.config.vk_format);
    vk_image_view_create_info.subresourceRange.baseMipLevel = config.vk_base_mip_level;   // default base mip is 0 automatically, and for custom it already has the value
    vk_image_view_create_info.subresourceRange.levelCount = config.vk_mips == vk_auto_mips ? image.config.vk_mips : config.vk_mips;
    vk_image_view_create_info.subresourceRange.baseArrayLayer = config.vk_base_array_layer;  // default base array layer is 0 automatically, and for custom it already has the value
    vk_image_view_create_info.subresourceRange.layerCount = config.vk_layers == vk_auto_layers ? image.config.vk_array_layers : config.vk_layers;

    auto result = vkCreateImageView(_vk_device, &vk_image_view_create_info, nullptr, &image_view.vk_image_view);
    if (result != VK_SUCCESS) {
        horizon_error("Failed to create image view");
        std::terminate();
    }

    image_view_handle_t handle = _image_views.size();
    while (_image_views.contains(handle)) {
        handle++;
    }

    _image_views.insert({handle, image_view});
    horizon_trace("created image_view");
    return handle;
}

void context_t::destroy_image_view(image_view_handle_t handle) {
    horizon_profile();
    assert(_image_views.contains(handle));
    image_view_t& image_view = _image_views[handle];
    vkDestroyImageView(_vk_device, image_view, nullptr);
    _image_views.erase(handle); 
}

descriptor_set_layout_handle_t context_t::create_descriptor_set_layout(const descriptor_set_layout_config_t& config) {
    horizon_profile();
    descriptor_set_layout_t descriptor_set_layout{ .config = config };
    
    VkDescriptorSetLayoutCreateInfo vk_descriptor_set_layout_create_info{};
    vk_descriptor_set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    vk_descriptor_set_layout_create_info.bindingCount = config.vk_descriptor_set_layout_bindings.size();
    vk_descriptor_set_layout_create_info.pBindings = config.vk_descriptor_set_layout_bindings.data();

    auto result = vkCreateDescriptorSetLayout(_vk_device, &vk_descriptor_set_layout_create_info, nullptr, &descriptor_set_layout.vk_descriptor_set_layout);
    if (result != VK_SUCCESS) {
        horizon_error("Failed to create descriptor set layout");
        std::terminate();
    }

    descriptor_set_layout_handle_t handle = _descriptor_set_layouts.size();
    while (_descriptor_set_layouts.contains(handle)) {
        handle++;
    }

    _descriptor_set_layouts.insert({handle, descriptor_set_layout});
    horizon_trace("created descriptor set layout");
    return handle;
}

void context_t::destroy_descriptor_set_layout(descriptor_set_layout_handle_t handle) {
    horizon_profile();
    assert(_descriptor_set_layouts.contains(handle));
    descriptor_set_layout_t& descriptor_set_layout = _descriptor_set_layouts[handle];
    vkDestroyDescriptorSetLayout(_vk_device, descriptor_set_layout, nullptr);
    _descriptor_set_layouts.erase(handle);   
}

descriptor_set_handle_t context_t::allocate_descriptor_set(descriptor_set_layout_handle_t descriptor_set_layout_handle) {
    horizon_profile();
    descriptor_set_t descriptor_set{ .descriptor_set_layout_handle = descriptor_set_layout_handle };
    assert(_descriptor_set_layouts.contains(descriptor_set_layout_handle));
    descriptor_set_layout_t& descriptor_set_layout = _descriptor_set_layouts[descriptor_set_layout_handle];

    VkDescriptorSetAllocateInfo vk_descriptor_set_allocate_info{};
    vk_descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    vk_descriptor_set_allocate_info.descriptorPool = _vk_descriptor_pool;
    vk_descriptor_set_allocate_info.descriptorSetCount = 1;
    VkDescriptorSetLayout vk_descriptor_set_layout_p[] = { descriptor_set_layout.vk_descriptor_set_layout }; 
    vk_descriptor_set_allocate_info.pSetLayouts = vk_descriptor_set_layout_p;

    auto result = vkAllocateDescriptorSets(_vk_device, &vk_descriptor_set_allocate_info, &descriptor_set.vk_descriptor_set);
    if (result != VK_SUCCESS) {
        horizon_error("Failed to allocate descriptor set");
        std::terminate();
    }

    descriptor_set_handle_t handle = _descriptor_sets.size();
    while (_descriptor_sets.contains(handle)) {
        handle++;
    }

    _descriptor_sets.insert({handle, descriptor_set});
    horizon_trace("created descriptor set");
    return handle;
}

void context_t::free_descriptor_set(descriptor_set_handle_t handle) {
    horizon_profile();
    assert(_descriptor_sets.contains(handle));
    descriptor_set_t& descriptor_set = _descriptor_sets[handle];
    vkFreeDescriptorSets(_vk_device, _vk_descriptor_pool, 1, &descriptor_set.vk_descriptor_set);
    _descriptor_sets.erase(handle);   
}

context_t::update_descriptor_t context_t::update_descriptor_set(descriptor_set_handle_t handle) {
    horizon_profile();
    return { .context = *this, .handle = handle };
}

semaphore_handle_t context_t::create_semaphore(const semaphore_config_t& config) {
    horizon_profile();
    semaphore_t semaphore{ .config = config };
    VkSemaphoreCreateInfo vk_semaphore_create_info{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    auto result = vkCreateSemaphore(_vk_device, &vk_semaphore_create_info, nullptr, &semaphore.vk_semaphore);
    if (result != VK_SUCCESS) {
        horizon_error("Failed to create semaphore");
        std::terminate();
    }
    semaphore_handle_t handle = _semaphores.size();
    while (_semaphores.contains(handle)) {
        handle++;
    }

    _semaphores.insert({handle, semaphore});
    horizon_trace("created semaphore");
    return handle;
}

void context_t::destroy_semaphore(semaphore_handle_t handle) {
    horizon_profile();
    assert(_semaphores.contains(handle));
    semaphore_t& semaphore = _semaphores[handle];
    vkDestroySemaphore(_vk_device, semaphore.vk_semaphore, nullptr);
    _semaphores.erase(handle);   
}

fence_handle_t context_t::create_fence(const fence_config_t& config) {
    horizon_profile();
    fence_t fence{ .config = config };
    VkFenceCreateInfo vk_fence_create_info{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    vk_fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    auto result = vkCreateFence(_vk_device, &vk_fence_create_info, nullptr, &fence.vk_fence);
    if (result != VK_SUCCESS) {
        horizon_error("Failed to create fence");
        std::terminate();
    }
    fence_handle_t handle = _fences.size();
    while (_fences.contains(handle)) {
        handle++;
    }

    _fences.insert({handle, fence});
    horizon_trace("created fence");
    return handle;
}

void context_t::destroy_fence(fence_handle_t handle) {
    horizon_profile();
    assert(_fences.contains(handle));
    fence_t& fence = _fences[handle];
    vkDestroyFence(_vk_device, fence.vk_fence, nullptr);
    _fences.erase(handle);   
}

command_pool_handle_t context_t::create_command_pool(const command_pool_config_t& config) {
    horizon_profile();
    command_pool_t command_pool{ .config = config };
    VkCommandPoolCreateInfo vk_command_pool_create_info{ .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    vk_command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vk_command_pool_create_info.queueFamilyIndex = _vk_graphics_queue.vk_index;
    auto result = vkCreateCommandPool(_vk_device, &vk_command_pool_create_info, nullptr, &command_pool.vk_command_pool);
    if (result != VK_SUCCESS) {
        horizon_error("Failed to create command pool");
        std::terminate();
    }
    command_pool_handle_t handle = _command_pools.size();
    while (_command_pools.contains(handle)) {
        handle++;
    }

    _command_pools.insert({handle, command_pool});
    horizon_trace("created command pool");
    return handle;
}

void context_t::destroy_command_pool(command_pool_handle_t handle) {
    horizon_profile();
    assert(_command_pools.contains(handle));
    command_pool_t& command_pool = _command_pools[handle];
    vkDestroyCommandPool(_vk_device, command_pool.vk_command_pool, nullptr);
    _command_pools.erase(handle); 
}

void context_t::exec_commands(VkCommandBuffer vk_commandbuffer, const command_list_t& command_list) {
    horizon_profile();
    // TODO: do this properly
    pipeline_handle_t current_active_pipeline_handle = std::numeric_limits<uint64_t>::max();
    for (auto& command : command_list._commands) {
        switch (command.type) {
            case command_type_t::e_bind_pipeline: 
                {
                    auto& pipeline = _pipelines[command.as.bind_pipeline.handle];
                    vkCmdBindPipeline(vk_commandbuffer, pipeline.vk_bind_point, pipeline);
                    current_active_pipeline_handle = command.as.bind_pipeline.handle;
                }
                break;
            case command_type_t::e_dispatch:
                {
                    const command_dispatch_t& dispatch = command.as.dispatch;
                    vkCmdDispatch(vk_commandbuffer, dispatch.x, dispatch.y, dispatch.z); 
                }
                break;
            case command_type_t::e_bind_descriptor_sets:
                {
                    assert(current_active_pipeline_handle != std::numeric_limits<uint32_t>::max());
                    const command_bind_descriptor_sets_t& bind_descriptor_set = command.as.bind_descriptor_sets;
                    VkDescriptorSet *sets = reinterpret_cast<VkDescriptorSet *>(alloca(bind_descriptor_set.count * sizeof(VkDescriptorSet)));
                    for (size_t i = 0; i < bind_descriptor_set.count; i++) {
                        assert(_descriptor_sets.contains(bind_descriptor_set.p_descriptors[i]));
                        auto& descriptor_set = _descriptor_sets[bind_descriptor_set.p_descriptors[i]];
                        sets[i] = descriptor_set;
                    }
                    assert(_pipelines.contains(current_active_pipeline_handle));
                    auto& pipeline = _pipelines[current_active_pipeline_handle];
                    vkCmdBindDescriptorSets(vk_commandbuffer, pipeline.vk_bind_point, pipeline.vk_layout, bind_descriptor_set.first_set, bind_descriptor_set.count, sets, 0, nullptr);
                }
                break;
            case command_type_t::e_push_constant:
                {
                    assert(current_active_pipeline_handle != std::numeric_limits<uint32_t>::max());
                    const command_push_constant_t& push_constant = command.as.push_constant;
                    assert(_pipelines.contains(current_active_pipeline_handle));
                    auto& pipeline = _pipelines[current_active_pipeline_handle];
                    vkCmdPushConstants(vk_commandbuffer, pipeline.vk_layout, push_constant.shader_stage, push_constant.offset, push_constant.size, push_constant.data);
                }
                break;
            case command_type_t::e_begin_rendering:
                {
                    const command_begin_rendering_t& begin_rendering = command.as.begin_rendering;
                    VkRenderingAttachmentInfo *p_color_attachment_infos = reinterpret_cast<VkRenderingAttachmentInfo *>(alloca(8 * sizeof(VkRenderingAttachmentInfo)));
                    VkRenderingAttachmentInfo depth_attachment_info;
                    for (size_t i = 0; i < begin_rendering.color_attachments_count; i++) {
                        assert(_image_views.contains(begin_rendering.p_color_attachments[i].image_view_handle));
                        auto& image_view = _image_views[begin_rendering.p_color_attachments[i].image_view_handle];
                        p_color_attachment_infos[i] = {};
                        p_color_attachment_infos[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                        p_color_attachment_infos[i].clearValue = begin_rendering.p_color_attachments[i].clear_value;
                        p_color_attachment_infos[i].imageLayout = begin_rendering.p_color_attachments[i].image_layout;
                        p_color_attachment_infos[i].loadOp = begin_rendering.p_color_attachments[i].load_op;
                        p_color_attachment_infos[i].storeOp = begin_rendering.p_color_attachments[i].store_op;
                        p_color_attachment_infos[i].imageView = image_view;
                    }
                    assert(_image_views.contains(begin_rendering.depth_attachment.image_view_handle));
                    auto& image_view = _image_views[begin_rendering.depth_attachment.image_view_handle];
                    depth_attachment_info = {};
                    depth_attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                    depth_attachment_info.clearValue = begin_rendering.depth_attachment.clear_value;
                    depth_attachment_info.imageLayout = begin_rendering.depth_attachment.image_layout;
                    depth_attachment_info.loadOp = begin_rendering.depth_attachment.load_op;
                    depth_attachment_info.storeOp = begin_rendering.depth_attachment.store_op;
                    depth_attachment_info.imageView = image_view;
                    
                    VkRenderingInfo rendering_info{ .sType = VK_STRUCTURE_TYPE_RENDERING_INFO };
                    rendering_info.renderArea = begin_rendering.render_area;
                    rendering_info.layerCount = begin_rendering.layer_count;
                    rendering_info.colorAttachmentCount = begin_rendering.color_attachments_count;
                    rendering_info.pColorAttachments = p_color_attachment_infos;
                    if (begin_rendering.use_depth) {
                        rendering_info.pDepthAttachment = &depth_attachment_info;
                    }
                    vkCmdBeginRendering(vk_commandbuffer, &rendering_info);
                }
                break;
            case command_type_t::e_end_rendering:
                {
                    vkCmdEndRendering(vk_commandbuffer);
                }
                break;
            case command_type_t::e_draw:
                {
                    const command_draw_t& draw = command.as.draw;
                    vkCmdDraw(vk_commandbuffer, draw.vertex_count, draw.instance_count, draw.first_vertex, draw.first_instance);
                }
                break;
            case command_type_t::e_image_memory_barrier:
                {
                    const command_image_memory_barrier_t& image_memory_barrier = command.as.image_memory_barrier;
                    assert(_images.contains(image_memory_barrier.image));
                    image_t& image = _images[image_memory_barrier.image];
                    
                    VkImageMemoryBarrier vk_image_memory_barrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
                    vk_image_memory_barrier.oldLayout = image_memory_barrier.old_image_layout;
                    vk_image_memory_barrier.newLayout = image_memory_barrier.new_image_layout;
                    
                    vk_image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;  // no queue transfer
                    vk_image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;  // no queue transfer

                    vk_image_memory_barrier.image = image;
                    vk_image_memory_barrier.subresourceRange.aspectMask = get_image_aspect(image.config.vk_format);
                    vk_image_memory_barrier.subresourceRange.baseMipLevel = 0;
                    vk_image_memory_barrier.subresourceRange.levelCount = 1;
                    vk_image_memory_barrier.subresourceRange.baseArrayLayer = 0;
                    vk_image_memory_barrier.subresourceRange.layerCount = 1;
                    vk_image_memory_barrier.srcAccessMask = image_memory_barrier.src_access_mask;
                    vk_image_memory_barrier.dstAccessMask = image_memory_barrier.dst_access_mask;

                    vkCmdPipelineBarrier(vk_commandbuffer, image_memory_barrier.src_pipeline_stage, image_memory_barrier.dst_pipeline_stage, 0, 0, nullptr, 0, nullptr, 1, &vk_image_memory_barrier);
                }
                break;
            case command_type_t::e_set_viewport_and_scissor:
                {
                    const command_set_viewport_and_scissor_t& set_viewport_and_scissor = command.as.set_viewport_and_scissor;
                    vkCmdSetViewport(vk_commandbuffer, 0, 1, &set_viewport_and_scissor.viewport);
                    vkCmdSetScissor(vk_commandbuffer, 0, 1, &set_viewport_and_scissor.scissor);
                }
                break;
            default:    
                horizon_error("Shouldnt reach here");
                std::terminate();
        }
    }
}

} // namespace gfx

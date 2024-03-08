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
    static std::set<VkFormat> depth_formats = {
        VK_FORMAT_D16_UNORM,
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
    };

    if (depth_formats.contains(format)) 
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    return VK_IMAGE_ASPECT_COLOR_BIT;
}

descriptor_set_layout_config_t& descriptor_set_layout_config_t::add_layout_binding(const VkDescriptorSetLayoutBinding& descriptor_set_layout_binding) {
    horizon_profile();
    descriptor_set_layout_bindings.push_back(descriptor_set_layout_binding);
    return *this;
}

descriptor_set_layout_config_t& descriptor_set_layout_config_t::add_layout_binding(uint32_t binding, VkDescriptorType descriptor_type, VkShaderStageFlags stage_flags, uint32_t count) {
    horizon_profile();
    VkDescriptorSetLayoutBinding descriptor_set_layout_binding{};
    descriptor_set_layout_binding.binding = binding;
    descriptor_set_layout_binding.descriptorType = descriptor_type;
    descriptor_set_layout_binding.descriptorCount = count;
    descriptor_set_layout_binding.stageFlags = stage_flags;
    descriptor_set_layout_bindings.push_back(descriptor_set_layout_binding);
    return *this;
}

pipeline_config_t::pipeline_config_t() {
    horizon_profile();
    pipeline_input_assembly_state = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, .primitiveRestartEnable = VK_FALSE };

    pipeline_depth_stencil_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    pipeline_depth_stencil_state_create_info.depthTestEnable = VK_TRUE;
    pipeline_depth_stencil_state_create_info.depthWriteEnable = VK_TRUE;
    pipeline_depth_stencil_state_create_info.depthCompareOp = VK_COMPARE_OP_LESS;
    pipeline_depth_stencil_state_create_info.depthBoundsTestEnable = VK_FALSE;
    pipeline_depth_stencil_state_create_info.minDepthBounds = 0.0f; // Optional
    pipeline_depth_stencil_state_create_info.maxDepthBounds = 1.0f; // Optional
    pipeline_depth_stencil_state_create_info.stencilTestEnable = VK_FALSE;
    pipeline_depth_stencil_state_create_info.front = {}; // Optional
    pipeline_depth_stencil_state_create_info.back = {}; // Optional
    
    pipeline_rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    pipeline_rasterization_state.depthClampEnable = VK_FALSE;
    pipeline_rasterization_state.rasterizerDiscardEnable = VK_FALSE;
    pipeline_rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
    pipeline_rasterization_state.lineWidth = 1.0f;
    pipeline_rasterization_state.cullMode = VK_CULL_MODE_NONE;
    pipeline_rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE;
    pipeline_rasterization_state.depthBiasEnable = VK_FALSE;
    pipeline_rasterization_state.depthBiasConstantFactor = 0.0f; // Optional
    pipeline_rasterization_state.depthBiasClamp = 0.0f; // Optional
    pipeline_rasterization_state.depthBiasSlopeFactor = 0.0f; // Optional

    pipeline_multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    pipeline_multisample_state.sampleShadingEnable = VK_FALSE;
    pipeline_multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    pipeline_multisample_state.minSampleShading = 1.0f; // Optional
    pipeline_multisample_state.pSampleMask = nullptr; // Optional
    pipeline_multisample_state.alphaToCoverageEnable = VK_FALSE; // Optional
    pipeline_multisample_state.alphaToOneEnable = VK_FALSE; // Optional

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

pipeline_config_t& pipeline_config_t::add_push_constant(uint32_t size, uint32_t offset, VkShaderStageFlags shader_stage) {
    horizon_profile();
    push_constant_ranges.push_back(VkPushConstantRange{ .stageFlags = shader_stage, .offset = offset, .size = size });
    return *this;
}

pipeline_config_t& pipeline_config_t::add_color_attachment(VkFormat format, const VkPipelineColorBlendAttachmentState& pipeline_color_blend_state) {
    horizon_profile();
    color_formats.push_back(format);
    pipeline_color_blend_attachment_states.push_back(pipeline_color_blend_state);
    return *this;
}

pipeline_config_t& pipeline_config_t::set_depth(VkFormat format, const VkPipelineDepthStencilStateCreateInfo& pipeline_color_blend_state) {
    horizon_profile();
    depth_format = format;
    this->pipeline_depth_stencil_state_create_info = pipeline_color_blend_state;
    return *this;
}

pipeline_config_t& pipeline_config_t::add_dynamic_state(const VkDynamicState& dynamic_state) {
    dynamic_states.push_back(dynamic_state);
    return *this;
}

pipeline_config_t& pipeline_config_t::add_vertex_input_binding_description(uint32_t binding, uint32_t stride, VkVertexInputRate input_rate) {
    horizon_profile();
    VkVertexInputBindingDescription vertex_input_binding_description{};
    vertex_input_binding_description.binding = binding;
    vertex_input_binding_description.stride = stride;
    vertex_input_binding_description.inputRate = input_rate;
    vertex_input_binding_descriptions.push_back(vertex_input_binding_description);
    return *this;
}

pipeline_config_t& pipeline_config_t::add_vertex_input_attribute_description(uint32_t binding, uint32_t location, VkFormat format, uint32_t offset) {
    horizon_profile();
    VkVertexInputAttributeDescription vertex_input_attribute_description{};
    vertex_input_attribute_description.binding = binding;
    vertex_input_attribute_description.format = format;
    vertex_input_attribute_description.location = location;
    vertex_input_attribute_description.offset = offset;
    vertex_input_attribute_descriptions.push_back(vertex_input_attribute_description);
    return *this;
}

pipeline_config_t& pipeline_config_t::set_pipeline_input_assembly_state(const VkPipelineInputAssemblyStateCreateInfo& pipeline_input_assembly_state) {
    horizon_profile();
    this->pipeline_input_assembly_state = pipeline_input_assembly_state;
    return *this;
}

pipeline_config_t& pipeline_config_t::set_pipeline_rasterization_state(const VkPipelineRasterizationStateCreateInfo& pipeline_rasterization_state) {
    horizon_profile();
    this->pipeline_rasterization_state = pipeline_rasterization_state;
    return *this;
}

pipeline_config_t& pipeline_config_t::set_pipeline_multisample_state(const VkPipelineMultisampleStateCreateInfo& pipeline_multisample_state) {
    horizon_profile();
    this->pipeline_multisample_state = pipeline_multisample_state;
    return *this;
}

VkPipelineColorBlendAttachmentState default_color_blend_attachment() {
    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
    return color_blend_attachment;
}

struct volk_initializer_t {
    volk_initializer_t() {
        horizon_profile();
        if (volkInitialize() != VK_SUCCESS) {
            horizon_error("Failed to initialize volk");
            exit(EXIT_FAILURE);
        }
    }
};

static volk_initializer_t volk_initializer{};

context_t::context_t(bool validation) : _validation(validation) {
    horizon_profile();
    create_instance();
    create_device();
    create_vma();
    create_command_pool();
    create_descriptor_pool();
}

context_t::~context_t() {
    horizon_profile();

    
    for (auto& [fence_handle, fence] : _fences) {
        vkDestroyFence(_device, fence, nullptr);
    }
    for (auto& [semaphore_handle, semaphore] : _semaphores) {
        vkDestroySemaphore(_device, semaphore, nullptr);
    }
    for (auto& [image_view_handle, image_view] : _image_views) {
        vkDestroyImageView(_device, image_view, nullptr);
    }
    for (auto& [image_handle, image] : _images) if (!image.from_swapchain) { 
        if (image.map) unmap_image(image_handle);
        vmaDestroyImage(_vma_allocator, image, image.allocation);
    }
    for (auto& [buffer_handle, buffer] : _buffers) {
        if (buffer.map) unmap_buffer(buffer_handle);
        vmaDestroyBuffer(_vma_allocator, buffer, buffer.allocation);
    }
    for (auto& [handle, descriptor_set_layout] : _descriptor_set_layouts) {
        vkDestroyDescriptorSetLayout(_device, descriptor_set_layout, nullptr);
    }
    for (auto& [pipeline_handle, pipeline] : _pipelines) {
        vkDestroyPipelineLayout(_device, pipeline.layout, nullptr);
        vkDestroyPipeline(_device, pipeline, nullptr);
    }
    for (auto& [shader_module_handle, shader_module] : _shader_modules) {
        vkDestroyShaderModule(_device, shader_module.module, nullptr);
    }
    vkDestroyDescriptorPool(_device, _descriptor_pool, nullptr);
    for (auto& [swapchain_handle, swapchain] : _swapchains) {
        vkb::destroy_swapchain(swapchain.swapchain);
        vkDestroySurfaceKHR(_instance, swapchain.surface, nullptr);
    }
    vkDestroyCommandPool(_device, _command_pool, nullptr);
    vmaDestroyAllocator(_vma_allocator);
    vkb::destroy_device(_device);
    vkb::destroy_instance(_instance);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
    void* p_user_data) {
    horizon_profile();
    switch (message_severity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: 
            horizon_trace("{}", p_callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: 
            horizon_info("{}", p_callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: 
            horizon_warn("{}", p_callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: 
            horizon_error("{}", p_callback_data->pMessage);
            break;
    }
    return VK_FALSE;
}

void context_t::create_instance() {
    horizon_profile();

    vkb::InstanceBuilder instance_builder{};
    instance_builder.set_debug_callback(debug_callback)
                    .desire_api_version(VK_API_VERSION_1_3)
                    .enable_extension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)
                    .request_validation_layers(_validation);
    auto result = instance_builder.build();
    if (!result) {
        horizon_error("Failed to create instance!");
        exit(EXIT_FAILURE);
    }
    _instance = result.value();
    volkLoadInstance(_instance);

    horizon_trace("created instance");
}

void context_t::create_device() {
    horizon_profile();
    vkb::PhysicalDeviceSelector physical_device_selector{_instance};
    physical_device_selector.add_required_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    physical_device_selector.add_required_extension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    physical_device_selector.add_required_extension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
    physical_device_selector.add_required_extension(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
    physical_device_selector.add_required_extension(VK_KHR_BIND_MEMORY_2_EXTENSION_NAME);
    physical_device_selector.add_required_extension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    VkPhysicalDeviceScalarBlockLayoutFeatures physical_device_scalar_block_layout_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES,
        .scalarBlockLayout = VK_TRUE,
    };
    physical_device_selector.add_required_extension_features<VkPhysicalDeviceScalarBlockLayoutFeatures>(physical_device_scalar_block_layout_features);
    VkPhysicalDeviceBufferDeviceAddressFeatures physical_device_buffer_device_address_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
        .bufferDeviceAddress = VK_TRUE,
        .bufferDeviceAddressCaptureReplay = VK_FALSE,
        .bufferDeviceAddressMultiDevice = VK_FALSE
    };
    physical_device_selector.add_required_extension_features<VkPhysicalDeviceBufferDeviceAddressFeatures>(physical_device_buffer_device_address_features);
    VkPhysicalDeviceDynamicRenderingFeaturesKHR physical_device_dynamic_rendering_features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES };
    physical_device_dynamic_rendering_features.dynamicRendering = VK_TRUE;
    physical_device_selector.add_required_extension_features<VkPhysicalDeviceDynamicRenderingFeaturesKHR>(physical_device_dynamic_rendering_features);

    physical_device_selector.prefer_gpu_device_type(vkb::PreferredDeviceType::integrated);

    // create temp window, to get surface
    core::window_t temp_window{"temp", 2, 2};

    VkSurfaceKHR temp_surface;
    {
        auto result = glfwCreateWindowSurface(_instance, temp_window.window(), nullptr, &temp_surface);
        if (result != VK_SUCCESS) {
            horizon_error("Failed to create surface");
            exit(EXIT_FAILURE);
        }
    }

    physical_device_selector.set_surface(temp_surface);

    {
        auto result = physical_device_selector.select(vkb::DeviceSelectionMode::only_fully_suitable);
        if (!result) {
            horizon_error("Failed to pick suitable physical device");
            exit(EXIT_FAILURE);
        }
        _physical_device = result.value();
    }

    vkb::DeviceBuilder device_builder{ _physical_device };
    {
        auto result = device_builder.build();
        if (!result) {
            horizon_error("Failed to create device!");
            exit(EXIT_FAILURE);
        }
        _device = result.value();
    }
    { 
        auto result =_device.get_queue(vkb::QueueType::graphics);
        if (!result) {
            horizon_error("Failed to get graphics queue");
            exit(EXIT_FAILURE);
        }
        _graphics_queue.queue = result.value();
    }
    {
        auto result = _device.get_queue_index(vkb::QueueType::graphics);
        if (!result) {
            horizon_error("Failed to get graphics queue index");
            exit(EXIT_FAILURE);
        }
        _graphics_queue.index = result.value();
    }
    { 
        auto result =_device.get_queue(vkb::QueueType::present);
        if (!result) {
            horizon_error("Failed to get present queue");
            exit(EXIT_FAILURE);
        }
        _present_queue.queue = result.value();
    }
    {
        auto result = _device.get_queue_index(vkb::QueueType::present);
        if (!result) {
            horizon_error("Failed to get present queue index");
            exit(EXIT_FAILURE);
        }
        _present_queue.index = result.value();
    }

    vkDestroySurfaceKHR(_instance, temp_surface, nullptr);
    
    volkLoadDevice(_device);

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
        .physicalDevice = _physical_device,
        .device = _device,
        .pVulkanFunctions = &vma_vulkan_functions,
        .instance = _instance,
        .vulkanApiVersion = VK_API_VERSION_1_3,
    };

    auto result = vmaCreateAllocator(&vma_allocator_create_info, &_vma_allocator);
    if (result != VK_SUCCESS) {
        horizon_error("Failed to create vma allocator");
        exit(EXIT_FAILURE);
    }
}

void context_t::create_command_pool() {
    horizon_profile();
    VkCommandPoolCreateInfo command_pool_create_info{};
    command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    command_pool_create_info.queueFamilyIndex = _graphics_queue.index;
    auto result = vkCreateCommandPool(_device, &command_pool_create_info, nullptr, &_command_pool);
    if (result != VK_SUCCESS) {
        horizon_error("Failed to create command pool");
        exit(EXIT_FAILURE);
    }
}

void context_t::create_descriptor_pool() {
    horizon_profile();
    std::vector<VkDescriptorPoolSize> pool_sizes{};

    {
        VkDescriptorPoolSize pool_size{};
        pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pool_size.descriptorCount = 1000;
        pool_sizes.push_back(pool_size);
    }

    {
        VkDescriptorPoolSize pool_size{};
        pool_size.type = VK_DESCRIPTOR_TYPE_SAMPLER;
        pool_size.descriptorCount = 1000;
        pool_sizes.push_back(pool_size);
    }

    {
        VkDescriptorPoolSize pool_size{};
        pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        pool_size.descriptorCount = 1000;
        pool_sizes.push_back(pool_size);
    }

    {
        VkDescriptorPoolSize pool_size{};
        pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        pool_size.descriptorCount = 1000;
        pool_sizes.push_back(pool_size);
    }

    {
        VkDescriptorPoolSize pool_size{};
        pool_size.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        pool_size.descriptorCount = 1000;
        pool_sizes.push_back(pool_size);
    }

    {
        VkDescriptorPoolSize pool_size{};
        pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pool_size.descriptorCount = 1000;
        pool_sizes.push_back(pool_size);
    }

    VkDescriptorPoolCreateInfo descriptor_pool_create_info{};
    descriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool_create_info.poolSizeCount = pool_sizes.size();
    descriptor_pool_create_info.pPoolSizes = pool_sizes.data();
    descriptor_pool_create_info.maxSets = 1000 * 7;
    descriptor_pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    if (vkCreateDescriptorPool(_device, &descriptor_pool_create_info, nullptr, &_descriptor_pool) != VK_SUCCESS) {
        horizon_error("Failed to create descriptor pool");
        exit(EXIT_FAILURE);
    }
    horizon_trace("created descriptor pool");
}

swapchain_handle_t context_t::create_swapchain(const core::window_t& window) {
    horizon_profile();
    swapchain_t swapchain{};
    {
        auto result = glfwCreateWindowSurface(_instance, window.window(), nullptr, &swapchain.surface);
        if (result != VK_SUCCESS) {
            horizon_error("Failed to create surface");
            exit(EXIT_FAILURE);
        }
    }
    {
        vkb::SwapchainBuilder swapchain_builder{_device, swapchain.surface};
        auto result = swapchain_builder.build();
        if (!result) {
            horizon_error("Failed to create swapchain");
            exit(EXIT_FAILURE);
        }
        swapchain.swapchain = result.value();    
    }

    swapchain_handle_t handle = _swapchains.size();
    while (_swapchains.contains(handle)) {
        handle++;
    }

    _swapchains.insert({handle, swapchain});
    
    horizon_trace("created swapchain with handle: {}", handle);
    
    std::vector<VkImage> images;
    {
        auto result = swapchain.swapchain.get_images();
        if (!result) {
            horizon_error("Failed to get swapchain images");
            exit(EXIT_FAILURE);
        }
        images = result.value();
    }

    for (auto image : images) {
        image_config_t config{};
        config.array_layers = 1;
        config.depth = 1;
        config.format = swapchain.swapchain.image_format;
        config.width = swapchain.swapchain.extent.width;
        config.height = swapchain.swapchain.extent.height;
        config.inital_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        config.mips = 1;
        config.sample_count = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
        config.tiling;
        config.type = VK_IMAGE_TYPE_2D;
        image_t _image{ .config = config };
        _image.image = image;
        _image.from_swapchain = true;

        image_handle_t image_handle = _images.size();
        while (_images.contains(image_handle)) {
            image_handle++;
        }

        _images.insert({image_handle, _image});

        auto handle = create_image_view(image_handle, {});

        swapchain.image_views.push_back(handle);
        swapchain.images.push_back(image_handle);
    }


    return handle;
}

void context_t::destroy_swapchain(swapchain_handle_t handle) {
    horizon_profile();
    assert(_swapchains.contains(handle));
    swapchain_t& swapchain = _swapchains[handle];
    vkDestroySwapchainKHR(_device, swapchain.swapchain, nullptr);
    vkDestroySurfaceKHR(_instance, swapchain.surface, nullptr);
    _swapchains.erase(handle);  
}

std::vector<image_handle_t> context_t::swapchain_images(swapchain_handle_t handle) {
    assert(_swapchains.contains(handle));
    swapchain_t& swapchain = _swapchains[handle];
    return swapchain.images;
}

std::vector<image_view_handle_t> context_t::swapchain_image_views(swapchain_handle_t handle) {
    assert(_swapchains.contains(handle));
    swapchain_t& swapchain = _swapchains[handle];
    return swapchain.image_views;
}

std::optional<uint32_t> context_t::acquire_swapchain_next_image_index(swapchain_handle_t handle, semaphore_handle_t semaphore_handle, fence_handle_t fence_handle) {
    assert(_swapchains.contains(handle));
    swapchain_t& swapchain = _swapchains[handle];
    VkSemaphore semaphore = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    if (semaphore_handle != null_handle) {
        assert(_semaphores.contains(semaphore_handle));
        semaphore_t& _semaphore = _semaphores[semaphore_handle];
        semaphore = _semaphore;
    }
    if (fence_handle != null_handle) {
        assert(_fences.contains(fence_handle));
        fence_t& _fence = _fences[fence_handle];
        fence = _fence;

    }
    uint32_t next_image;
    {
        auto result = vkAcquireNextImageKHR(_device, swapchain.swapchain, UINT64_MAX, semaphore, fence, &next_image);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            return std::nullopt;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            horizon_error("Failed to acquire swap chain image");
            exit(EXIT_FAILURE);
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

    auto shader_module = shaderc_compiler.CompileGlslToSpv(preprocessed_code, shaderc_kind, config.name.c_str(), shaderc_compile_options);
    if (shader_module.GetCompilationStatus() != shaderc_compilation_status_success) {
        horizon_error("{}", shader_module.GetErrorMessage());
        exit(EXIT_FAILURE);
    }
    
    std::vector<uint32_t> shader_module_code = { shader_module.begin(), shader_module.end() };

    VkShaderModuleCreateInfo shader_module_create_info{ .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    shader_module_create_info.codeSize = shader_module_code.size() * 4;
    shader_module_create_info.pCode = shader_module_code.data();

    shader_module_t _shader_module{ .config = config };

    auto result = vkCreateShaderModule(_device, &shader_module_create_info, nullptr, &_shader_module.module);
    if (result != VK_SUCCESS) {
        horizon_error("Failed to create shader module");
        exit(EXIT_FAILURE);
    }

    shader_module_handle_t handle = _shader_modules.size();
    while (_shader_modules.contains(handle)) {
        handle++;
    }

    _shader_modules.insert({handle, _shader_module});

    horizon_trace("shader module created with name: {}", config.name);
    return handle;
}

void context_t::destroy_shader_module(shader_module_handle_t handle) {
    horizon_profile();
    assert(_shader_modules.contains(handle));
    VkShaderModule shader_module = _shader_modules[handle];
    vkDestroyShaderModule(_device, shader_module, nullptr);
    _shader_modules.erase(handle);    
}

pipeline_handle_t context_t::create_compute_pipeline(const pipeline_config_t& config) {
    horizon_profile();
    assert(config.shaders.size() == 1);  // for compute it should only be 1
    pipeline_t pipeline{ .config = config, .bind_point = VK_PIPELINE_BIND_POINT_COMPUTE };

    // maybe move this to its own function
    VkDescriptorSetLayout *set_layouts = reinterpret_cast<VkDescriptorSetLayout *>(alloca(config.descriptor_set_layouts.size() * sizeof(VkDescriptorSetLayout)));
    for (size_t i = 0; i < config.descriptor_set_layouts.size(); i++) {
        assert(_descriptor_set_layouts.contains(config.descriptor_set_layouts[i]));
        auto& descriptor_set_layout = _descriptor_set_layouts[config.descriptor_set_layouts[i]];
        set_layouts[i] = descriptor_set_layout;
    }
    VkPipelineLayoutCreateInfo pipeline_layout_create_info{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipeline_layout_create_info.setLayoutCount = config.descriptor_set_layouts.size();
    pipeline_layout_create_info.pSetLayouts = set_layouts;
    pipeline_layout_create_info.pushConstantRangeCount = config.push_constant_ranges.size(); 
    pipeline_layout_create_info.pPushConstantRanges = config.push_constant_ranges.data();
    {
        auto result = vkCreatePipelineLayout(_device, &pipeline_layout_create_info, nullptr, &pipeline.layout);
        if (result != VK_SUCCESS) {
            horizon_error("Failed to create pipeline layout");
            exit(EXIT_FAILURE);
        }
    }

    // maybe move this to its own function
    VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    pipeline_shader_stage_create_info.pName = "main";
    pipeline_shader_stage_create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipeline_shader_stage_create_info.module = _shader_modules[config.shaders[0]];

    VkComputePipelineCreateInfo compute_pipeline_create_info{ .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    compute_pipeline_create_info.layout = pipeline.layout;
    compute_pipeline_create_info.stage = pipeline_shader_stage_create_info;
    {
        auto result = vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &compute_pipeline_create_info, nullptr, &pipeline.pipeline);
        if (result != VK_SUCCESS) {
            horizon_error("Failed to create compute pipeline");
            exit(EXIT_FAILURE);
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
    pipeline_t pipeline{ .config = config, .bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS };

    VkPipelineDynamicStateCreateInfo dynamic_state{ .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamic_state.dynamicStateCount = config.dynamic_states.size();
    dynamic_state.pDynamicStates = config.dynamic_states.data();

    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = config.vertex_input_binding_descriptions.size();
    vertex_input_info.pVertexBindingDescriptions = config.vertex_input_binding_descriptions.data();
    vertex_input_info.vertexAttributeDescriptionCount = config.vertex_input_attribute_descriptions.size();
    vertex_input_info.pVertexAttributeDescriptions = config.vertex_input_attribute_descriptions.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly = config.pipeline_input_assembly_state;

    VkPipelineRasterizationStateCreateInfo rasterizer = config.pipeline_rasterization_state;

    VkPipelineMultisampleStateCreateInfo multisampling = config.pipeline_multisample_state;

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = VK_LOGIC_OP_COPY; // Optional
    color_blending.attachmentCount = config.pipeline_color_blend_attachment_states.size();
    color_blending.pAttachments = config.pipeline_color_blend_attachment_states.data();
    color_blending.blendConstants[0] = 0.0f; // Optional
    color_blending.blendConstants[1] = 0.0f; // Optional
    color_blending.blendConstants[2] = 0.0f; // Optional
    color_blending.blendConstants[3] = 0.0f; // Optional

    VkPipelineDepthStencilStateCreateInfo depth_stencil = config.pipeline_depth_stencil_state_create_info;

    std::vector<VkPipelineShaderStageCreateInfo> pipeline_shader_stage_create_infos;

    for (auto& shader_module_handle : config.shaders) {
        assert(_shader_modules.contains(shader_module_handle));
        auto& shader_module = _shader_modules[shader_module_handle];

        VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        pipeline_shader_stage_create_info.pName = "main";
        switch (shader_module.config.type) {
            case shader_type_t::e_fragment:
                pipeline_shader_stage_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                break;
            case shader_type_t::e_vertex:
                pipeline_shader_stage_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
                break;
            default:
                horizon_error("shouldnt reach here");
                exit(EXIT_FAILURE);
        }
        assert(_shader_modules.contains(shader_module_handle));
        pipeline_shader_stage_create_info.module = _shader_modules[shader_module_handle];
        pipeline_shader_stage_create_infos.push_back(pipeline_shader_stage_create_info);
    }

    // maybe move this to its own function
    VkDescriptorSetLayout *set_layouts = reinterpret_cast<VkDescriptorSetLayout *>(alloca(config.descriptor_set_layouts.size() * sizeof(VkDescriptorSetLayout)));
    for (size_t i = 0; i < config.descriptor_set_layouts.size(); i++) {
        assert(_descriptor_set_layouts.contains(config.descriptor_set_layouts[i]));
        auto& descriptor_set_layout = _descriptor_set_layouts[config.descriptor_set_layouts[i]];
        set_layouts[i] = descriptor_set_layout;
    }
    VkPipelineLayoutCreateInfo pipeline_layout_create_info{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipeline_layout_create_info.setLayoutCount = config.descriptor_set_layouts.size();
    pipeline_layout_create_info.pSetLayouts = set_layouts;
    pipeline_layout_create_info.pushConstantRangeCount = config.push_constant_ranges.size(); 
    pipeline_layout_create_info.pPushConstantRanges = config.push_constant_ranges.data();
    {
        auto result = vkCreatePipelineLayout(_device, &pipeline_layout_create_info, nullptr, &pipeline.layout);
        if (result != VK_SUCCESS) {
            horizon_error("Failed to create pipeline layout");
            exit(EXIT_FAILURE);
        }
    }

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)5;
    viewport.height = (float)5;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = { 5, 5 };

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    VkPipelineRenderingCreateInfoKHR pipeline_create{ .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR };
    pipeline_create.pNext                   = nullptr;
    pipeline_create.colorAttachmentCount    = config.color_formats.size();
    pipeline_create.pColorAttachmentFormats = config.color_formats.data();
    pipeline_create.depthAttachmentFormat   = config.depth_format;
    pipeline_create.stencilAttachmentFormat = config.depth_format;

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = static_cast<uint32_t>(config.shaders.size());
    pipeline_info.pStages = pipeline_shader_stage_create_infos.data();
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = pipeline.layout;
    pipeline_info.renderPass = VK_NULL_HANDLE;
    pipeline_info.subpass = 0;
    pipeline_info.pNext = &pipeline_create;

    auto result = vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline.pipeline);
    if (result != VK_SUCCESS) {
        horizon_error("Failed to create graphics pipeline");
        exit(EXIT_FAILURE);
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
    vkDestroyPipelineLayout(_device, pipeline.layout, nullptr);
    vkDestroyPipeline(_device, pipeline, nullptr);
    _pipelines.erase(handle);    
}

buffer_handle_t context_t::create_buffer(const buffer_config_t& config) {
    horizon_profile();
    VkBufferCreateInfo buffer_create_info{ .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    buffer_create_info.size = config.size;
    buffer_create_info.usage = config.usage_flags;

    VmaAllocationCreateInfo vma_allocation_create_info{};
    vma_allocation_create_info.usage = config.vma_memory_usage;
    vma_allocation_create_info.flags = config.vma_allocation_create_flags;

    buffer_t buffer{ .config = config };

    auto result = vmaCreateBuffer(_vma_allocator, &buffer_create_info, &vma_allocation_create_info, &buffer.buffer, &buffer.allocation, nullptr);
    if (result != VK_SUCCESS) {
        horizon_error("Failed to create buffer");
        exit(EXIT_FAILURE);
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
    vmaDestroyBuffer(_vma_allocator, buffer, buffer.allocation);
    _buffers.erase(handle);    
}

void *context_t::map_buffer(buffer_handle_t handle) {
    horizon_profile();
    assert(_buffers.contains(handle));
    buffer_t& buffer = _buffers[handle];
    vmaMapMemory(_vma_allocator, buffer.allocation, &buffer.map);
    return buffer.map;
}

void context_t::unmap_buffer(buffer_handle_t handle) {
    horizon_profile();
    assert(_buffers.contains(handle));
    buffer_t& buffer = _buffers[handle];
    assert(buffer.map);
    vmaUnmapMemory(_vma_allocator, buffer.allocation);
}

image_handle_t context_t::create_image(const image_config_t& config) {
    horizon_profile();
    VkImageCreateInfo image_create_info{ .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    image_create_info.imageType = config.type;
    image_create_info.extent = { config.width, config.height, config.depth };
    
    if (config.mips == auto_calculate_mip_levels) {
        VkImageFormatProperties image_format_properties{};
        vkGetPhysicalDeviceImageFormatProperties(_physical_device, VK_FORMAT_R8G8B8A8_SRGB, VkImageType::VK_IMAGE_TYPE_2D, VkImageTiling::VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 0, &image_format_properties);
        image_create_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(std::max(config.width, config.height), config.depth)))) + 1;  // this might be wrong ?
        image_create_info.mipLevels = std::min(image_format_properties.maxMipLevels, image_create_info.mipLevels); 
    } else {
        image_create_info.mipLevels = config.mips;
    }

    image_create_info.arrayLayers = config.array_layers;
    image_create_info.format = config.format;
    image_create_info.tiling = config.tiling;
    image_create_info.initialLayout = config.inital_layout;
    image_create_info.usage = config.usage;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_create_info.samples = config.sample_count;
    image_create_info.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

    image_t image{ .config = config };
    image.config.mips = image_create_info.mipLevels;

    VmaAllocationCreateInfo vma_allocation_create_info{};
    vma_allocation_create_info.usage = config.vma_memory_usage;
    vma_allocation_create_info.flags = config.vma_allocation_create_flags;

    auto result = vmaCreateImage(_vma_allocator, &image_create_info, &vma_allocation_create_info, &image.image, &image.allocation, nullptr);
    if (result != VK_SUCCESS) {
        horizon_error("Failed to create image");
        exit(EXIT_FAILURE);
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
    vmaDestroyImage(_vma_allocator, image, image.allocation);
    _images.erase(handle);    
}

void *context_t::map_image(image_handle_t handle) {
    horizon_profile();
    assert(_images.contains(handle));
    image_t& image = _images[handle];
    vmaMapMemory(_vma_allocator, image.allocation, &image.map);
    return image.map;
}

void context_t::unmap_image(image_handle_t handle) {
    horizon_profile();
    assert(_images.contains(handle));
    image_t& image = _images[handle];
    assert(image.map);
    vmaUnmapMemory(_vma_allocator, image.allocation);
}

image_view_handle_t context_t::create_image_view(image_handle_t image_handle, const image_view_config_t& config) {
    assert(_images.contains(image_handle));
    auto& image = _images[image_handle];
    
    image_view_t image_view{ .config = config, .image_handle = image_handle };

    VkImageViewType image_view_type;
    if (config.image_view_type == auto_image_view_type) {
        if (image.config.type == VkImageType::VK_IMAGE_TYPE_1D) image_view_type = VkImageViewType::VK_IMAGE_VIEW_TYPE_1D;
        if (image.config.type == VkImageType::VK_IMAGE_TYPE_2D) image_view_type = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
        if (image.config.type == VkImageType::VK_IMAGE_TYPE_3D) image_view_type = VkImageViewType::VK_IMAGE_VIEW_TYPE_3D;
    } else {
        image_view_type = config.image_view_type;
    }

    VkImageViewCreateInfo image_view_create_info{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    image_view_create_info.image = image;
    image_view_create_info.viewType = image_view_type;
    image_view_create_info.format = config.format == auto_image_format ? image.config.format : config.format;
    image_view_create_info.subresourceRange.aspectMask = get_image_aspect(image.config.format);
    image_view_create_info.subresourceRange.baseMipLevel = config.base_mip_level;   // default base mip is 0 automatically, and for custom it already has the value
    image_view_create_info.subresourceRange.levelCount = config.mips == auto_mips ? image.config.mips : config.mips;
    image_view_create_info.subresourceRange.baseArrayLayer = config.base_array_layer;  // default base array layer is 0 automatically, and for custom it already has the value
    image_view_create_info.subresourceRange.layerCount = config.layers == auto_layers ? image.config.array_layers : config.layers;

    auto result = vkCreateImageView(_device, &image_view_create_info, nullptr, &image_view.image_view);
    if (result != VK_SUCCESS) {
        horizon_error("Failed to create image view");
        exit(EXIT_FAILURE);
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
    vkDestroyImageView(_device, image_view, nullptr);
    _image_views.erase(handle); 
}

descriptor_set_layout_handle_t context_t::create_descriptor_set_layout(const descriptor_set_layout_config_t& config) {
    horizon_profile();
    descriptor_set_layout_t descriptor_set_layout{ .config = config };
    
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info{};
    descriptor_set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_set_layout_create_info.bindingCount = config.descriptor_set_layout_bindings.size();
    descriptor_set_layout_create_info.pBindings = config.descriptor_set_layout_bindings.data();

    auto result = vkCreateDescriptorSetLayout(_device, &descriptor_set_layout_create_info, nullptr, &descriptor_set_layout.descriptor_set_layout);
    if (result != VK_SUCCESS) {
        horizon_error("Failed to create descriptor set layout");
        exit(EXIT_FAILURE);
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
    vkDestroyDescriptorSetLayout(_device, descriptor_set_layout, nullptr);
    _descriptor_set_layouts.erase(handle);   
}

descriptor_set_handle_t context_t::create_descriptor_set(descriptor_set_layout_handle_t descriptor_set_layout_handle) {
    horizon_profile();
    descriptor_set_t descriptor_set{ .descriptor_set_layout_handle = descriptor_set_layout_handle };
    assert(_descriptor_set_layouts.contains(descriptor_set_layout_handle));
    descriptor_set_layout_t& descriptor_set_layout = _descriptor_set_layouts[descriptor_set_layout_handle];

    VkDescriptorSetAllocateInfo descriptor_set_allocate_info{};
    descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptor_set_allocate_info.descriptorPool = _descriptor_pool;
    descriptor_set_allocate_info.descriptorSetCount = 1;
    VkDescriptorSetLayout descriptor_set_layout_p[] = { descriptor_set_layout.descriptor_set_layout }; 
    descriptor_set_allocate_info.pSetLayouts = descriptor_set_layout_p;

    auto result = vkAllocateDescriptorSets(_device, &descriptor_set_allocate_info, &descriptor_set.descriptor_set);
    if (result != VK_SUCCESS) {
        horizon_error("Failed to allocate descriptor set");
        exit(EXIT_FAILURE);
    }

    descriptor_set_handle_t handle = _descriptor_sets.size();
    while (_descriptor_sets.contains(handle)) {
        handle++;
    }

    _descriptor_sets.insert({handle, descriptor_set});
    horizon_trace("created descriptor set");
    return handle;
}

void context_t::destroy_descriptor_set(descriptor_set_handle_t handle) {
    horizon_profile();
    assert(_descriptor_sets.contains(handle));
    descriptor_set_t& descriptor_set = _descriptor_sets[handle];
    vkFreeDescriptorSets(_device, _descriptor_pool, 1, &descriptor_set.descriptor_set);
    _descriptor_sets.erase(handle);   
}

context_t::update_descriptor_t context_t::update_descriptor_set(descriptor_set_handle_t handle) {
    horizon_profile();
    return { .context = *this, .handle = handle };
}

semaphore_handle_t context_t::create_semaphore(const semaphore_config_t& config) {
    horizon_profile();
    semaphore_t semaphore{ .config = config };
    VkSemaphoreCreateInfo semaphore_create_info{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    auto result = vkCreateSemaphore(_device, &semaphore_create_info, nullptr, &semaphore.semaphore);
    if (result != VK_SUCCESS) {
        horizon_error("Failed to create info");
        exit(EXIT_FAILURE);
    }
    semaphore_handle_t handle = _semaphores.size();
    while (_semaphores.contains(handle)) {
        handle++;
    }

    _semaphores.insert({handle, semaphore});
    horizon_trace("created descriptor set");
    return handle;
}

void context_t::destroy_semaphore(semaphore_handle_t handle) {
    horizon_profile();
    assert(_semaphores.contains(handle));
    semaphore_t& semaphore = _semaphores[handle];
    vkDestroySemaphore(_device, semaphore.semaphore, nullptr);
    _semaphores.erase(handle);   
}

fence_handle_t context_t::create_fence(const fence_config_t& config) {
    horizon_profile();
    fence_t fence{ .config = config };
    VkFenceCreateInfo fence_create_info{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    auto result = vkCreateFence(_device, &fence_create_info, nullptr, &fence.fence);
    if (result != VK_SUCCESS) {
        horizon_error("Failed to create info");
        exit(EXIT_FAILURE);
    }
    fence_handle_t handle = _fences.size();
    while (_fences.contains(handle)) {
        handle++;
    }

    _fences.insert({handle, fence});
    horizon_trace("created descriptor set");
    return handle;
}

void context_t::destroy_fence(fence_handle_t handle) {
    horizon_profile();
    assert(_fences.contains(handle));
    fence_t& fence = _fences[handle];
    vkDestroyFence(_device, fence.fence, nullptr);
    _fences.erase(handle);   
}

void context_t::exec_commands(VkCommandBuffer commandbuffer, const std::vector<command_t>& commands) {
    horizon_profile();
    // TODO: do it properly
    pipeline_handle_t current_active_pipeline_handle = std::numeric_limits<uint64_t>::max();
    for (auto& command : commands) {
        switch (command.type) {
            case command_type_t::e_bind_pipeline: 
                {
                    auto& pipeline = _pipelines[command.as.bind_pipeline.handle];
                    vkCmdBindPipeline(commandbuffer, pipeline.bind_point, pipeline);
                    current_active_pipeline_handle = command.as.bind_pipeline.handle;
                }
                break;
            case command_type_t::e_dispatch:
                {
                    const command_dispatch_t& dispatch = command.as.dispatch;
                    vkCmdDispatch(commandbuffer, dispatch.x, dispatch.y, dispatch.z); 
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
                    vkCmdBindDescriptorSets(commandbuffer, pipeline.bind_point, pipeline.layout, bind_descriptor_set.first_set, bind_descriptor_set.count, sets, 0, nullptr);
                }
                break;
            case command_type_t::e_push_constant:
                {
                    assert(current_active_pipeline_handle != std::numeric_limits<uint32_t>::max());
                    const command_push_constant_t& push_constant = command.as.push_constant;
                    assert(_pipelines.contains(current_active_pipeline_handle));
                    auto& pipeline = _pipelines[current_active_pipeline_handle];
                    vkCmdPushConstants(commandbuffer, pipeline.layout, push_constant.shader_stage, push_constant.offset, push_constant.size, push_constant.data);
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
                    vkCmdBeginRendering(commandbuffer, &rendering_info);
                }
                break;
            case command_type_t::e_end_rendering:
                {
                    vkCmdEndRendering(commandbuffer);
                }
                break;
            case command_type_t::e_draw:
                {
                    const command_draw_t& draw = command.as.draw;
                    vkCmdDraw(commandbuffer, draw.vertex_count, draw.instance_count, draw.first_vertex, draw.first_instance);
                }
                break;
            default:    
                horizon_error("Shouldnt reach here");
                exit(EXIT_FAILURE);
        }
    }
}

} // namespace gfx

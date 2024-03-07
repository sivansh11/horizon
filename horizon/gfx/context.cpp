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

namespace gfx {

descriptor_set_layout_config_t& descriptor_set_layout_config_t::add_layout_binding(const VkDescriptorSetLayoutBinding& descriptor_set_layout_binding) {
    descriptor_set_layout_bindings.push_back(descriptor_set_layout_binding);
    return *this;
}

descriptor_set_layout_config_t& descriptor_set_layout_config_t::add_layout_binding(uint32_t binding, VkDescriptorType descriptor_type, VkShaderStageFlags stage_flags, uint32_t count) {
    VkDescriptorSetLayoutBinding descriptor_set_layout_binding{};
    descriptor_set_layout_binding.binding = binding;
    descriptor_set_layout_binding.descriptorType = descriptor_type;
    descriptor_set_layout_binding.descriptorCount = count;
    descriptor_set_layout_binding.stageFlags = stage_flags;
    descriptor_set_layout_bindings.push_back(descriptor_set_layout_binding);
    return *this;
}

pipeline_config_t& pipeline_config_t::add_shader(shader_module_handle_t handle) {
    shaders.push_back(handle);
    return *this;
}

pipeline_config_t& pipeline_config_t::add_descriptor_set_layout(descriptor_set_layout_handle_t handle) {
    descriptor_set_layouts.push_back(handle);
    return *this;
}

pipeline_config_t& pipeline_config_t::add_push_constant(uint32_t size, uint32_t offset, VkShaderStageFlags shader_stage) {
    push_constant_ranges.push_back(VkPushConstantRange{ .stageFlags = shader_stage, .offset = offset, .size = size });
    return *this;
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

swapchain_handle_t context_t::create_swapchain(const core::window_t& window) {
    horizon_profile();
    swapchain_t swapchain;
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

    return handle;
}

void context_t::create_vma() {
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

shader_module_handle_t context_t::create_shader_module(const std::string& code, shader_type_t shader_type, const std::string& name) {
    horizon_profile();
    shaderc_shader_kind shaderc_kind;

    if (shader_type == shader_type_t::e_vertex) shaderc_kind = shaderc_vertex_shader;
    if (shader_type == shader_type_t::e_fragment) shaderc_kind = shaderc_fragment_shader;
    if (shader_type == shader_type_t::e_compute) shaderc_kind = shaderc_compute_shader;
    
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

    auto preprocess = shaderc_compiler.PreprocessGlsl(code, shaderc_kind, name.c_str(), shaderc_compile_options);
    std::string preprocessed_code = { preprocess.begin(), preprocess.end() };

    auto shader_module = shaderc_compiler.CompileGlslToSpv(preprocessed_code, shaderc_kind, name.c_str(), shaderc_compile_options);
    if (shader_module.GetCompilationStatus() != shaderc_compilation_status_success) {
        horizon_error("{}", shader_module.GetErrorMessage());
        exit(EXIT_FAILURE);
    }
    
    std::vector<uint32_t> shader_module_code = { shader_module.begin(), shader_module.end() };

    VkShaderModuleCreateInfo shader_module_create_info{ .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    shader_module_create_info.codeSize = shader_module_code.size() * 4;
    shader_module_create_info.pCode = shader_module_code.data();

    shader_module_t _shader_module{};

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

    horizon_trace("shader module created with name: {}", name);
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

// image_handle_t context_t::create_image(const image_config_t& config) {
//     horizon_profile();
//     VkImageCreateInfo image_create_info{ .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
//     image_create_info.size = config.size;
//     image_create_info.usage = config.usage_flags;

//     VmaAllocationCreateInfo vma_allocation_create_info{};
//     vma_allocation_create_info.usage = config.vma_memory_usage;
//     vma_allocation_create_info.flags = config.vma_allocation_create_flags;

//     image_t image{ .config = config };

//     auto result = vmaCreateBuffer(_vma_allocator, &image_create_info, &vma_allocation_create_info, &image.image, &image.allocation, nullptr);
//     if (result != VK_SUCCESS) {
//         horizon_error("Failed to create image");
//         exit(EXIT_FAILURE);
//     }

//     image_handle_t handle = _images.size();
//     while (_images.contains(handle)) {
//         handle++;
//     }

//     _images.insert({handle, image});
//     horizon_trace("created image");
//     return handle;
// }

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

void context_t::exec_commands(VkCommandBuffer commandbuffer, const std::vector<command_t>& commands) {
    horizon_profile();
    for (auto& command : commands) {
        switch (command.type) {
            case command_type_t::e_bind_pipeline: 
                {
                    auto& pipeline = _pipelines[command.as.bind_pipeline.handle];
                    vkCmdBindPipeline(commandbuffer, pipeline.bind_point, pipeline);
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
                    const command_bind_descriptor_sets_t& bind_descriptor_set = command.as.bind_descriptor_sets;
                    VkDescriptorSet *sets = reinterpret_cast<VkDescriptorSet *>(alloca(bind_descriptor_set.count * sizeof(VkDescriptorSet)));
                    for (size_t i = 0; i < bind_descriptor_set.count; i++) {
                        assert(_descriptor_sets.contains(bind_descriptor_set.p_descriptors[i]));
                        auto& descriptor_set = _descriptor_sets[bind_descriptor_set.p_descriptors[i]];
                        sets[i] = descriptor_set;
                    }
                    assert(_pipelines.contains(bind_descriptor_set.pipeline_handle));
                    auto& pipeline = _pipelines[bind_descriptor_set.pipeline_handle];
                    vkCmdBindDescriptorSets(commandbuffer, pipeline.bind_point, pipeline.layout, bind_descriptor_set.first_set, bind_descriptor_set.count, sets, 0, nullptr);
                }
                break;
            case command_type_t::e_push_constant:
                {
                    const command_push_constant_t& push_constant = command.as.push_constant;
                    assert(_pipelines.contains(push_constant.pipeline_handle));
                    auto& pipeline = _pipelines[push_constant.pipeline_handle];
                    vkCmdPushConstants(commandbuffer, pipeline.layout, push_constant.shader_stage, push_constant.offset, push_constant.size, push_constant.data);
                }
                break;
            default:    
                horizon_error("Shouldnt reach here");
                exit(EXIT_FAILURE);
        }
    }
}

} // namespace gfx

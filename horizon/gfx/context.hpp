#ifndef GFX_CONTEXT_HPP
#define GFX_CONTEXT_HPP

#include "core/core.hpp"

#include <volk.h>
#include <VkBootstrap.h>
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#define VK_NO_PROTOTYPE
#include <vk_mem_alloc.h>

#include <cstdint>
#include <map>

#define vk_check(result, msg) \
do {                              \
    if (result != VK_SUCCESS) {   \
        horizon_error(msg);       \
        exit(EXIT_FAILURE);       \
    }                             \
} while (false)

#define handle(name) \
struct name { \
    uint64_t val; \
    name() = default; \
    name(uint64_t val) : val(val) {} \
    constexpr name& operator = (const name& new_val) = default; \
    bool operator<(const name& other) const { \
        return val < other.val; \
    } \
    operator uint64_t() { \
        return val; \
    } \
    name& operator++(int) { \
        val++; \
        return *this; \
    } \
    name& operator++() { \
        val++; \
        return *this; \
    } \
    std::ostream& operator<<(std::ostream& o) { \
        o << val; \
        return o; \
    } \
}

namespace core {

class window_t;

} // namespace core

namespace gfx {

handle(swapchain_handle_t);
handle(shader_module_handle_t);
handle(pipeline_handle_t);
handle(buffer_handle_t);
handle(image_handle_t);
handle(descriptor_set_layout_handle_t);
handle(descriptor_set_handle_t);

struct queue_t {
    VkQueue queue;
    uint32_t index;
    operator VkQueue() {
        return queue;
    }
};

enum class shader_type_t {
    e_vertex,
    e_fragment,
    e_compute,
};

struct shader_module_t {
    VkShaderModule module;
    shader_type_t type;
    operator VkShaderModule() {
        return module;
    }
};

struct swapchain_t {
    VkSurfaceKHR surface;
    vkb::Swapchain swapchain;
    operator vkb::Swapchain() {
        return swapchain;
    }
};

// TODO: add builder style add{thing}
struct pipeline_config_t {
    pipeline_config_t& add_shader(shader_module_handle_t handle);
    pipeline_config_t& add_descriptor_set_layout(descriptor_set_layout_handle_t handle);
    pipeline_config_t& add_push_constant(uint32_t size, uint32_t offset, VkShaderStageFlags shader_stage);

    std::vector<shader_module_handle_t> shaders;
    std::vector<descriptor_set_layout_handle_t> descriptor_set_layouts;
    std::vector<VkPushConstantRange> push_constant_ranges;
};

struct pipeline_t {
    VkPipeline pipeline;
    pipeline_config_t config;
    VkPipelineLayout layout;
    VkPipelineBindPoint bind_point;
    operator VkPipeline() {
        return pipeline;
    }
};

struct buffer_config_t {
    size_t size;
    VkBufferUsageFlags usage_flags;
    VmaAllocationCreateFlags vma_allocation_create_flags;
    VmaMemoryUsage vma_memory_usage = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO;
};

struct buffer_t {
    VkBuffer buffer;
    VmaAllocation allocation;
    buffer_config_t config;
    void *map = nullptr;
    operator VkBuffer() {
        return buffer;
    }
};

struct image_config_t {
    // size_t size;
    // VkBufferUsageFlags usage_flags;
    // VmaAllocationCreateFlags vma_allocation_create_flags;
    // VmaMemoryUsage vma_memory_usage = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO;
};

struct image_t {
    VkImage image;
    VmaAllocation allocation;
    image_config_t config;
    void *map = nullptr;
    operator VkImage() {
        return image;
    }
};

struct descriptor_set_layout_config_t {
    descriptor_set_layout_config_t& add_layout_binding(const VkDescriptorSetLayoutBinding& descriptor_set_layout_binding);
    descriptor_set_layout_config_t& add_layout_binding(uint32_t binding, VkDescriptorType descriptor_type, VkShaderStageFlags stage_flags, uint32_t count = 1);
    
    std::vector<VkDescriptorSetLayoutBinding> descriptor_set_layout_bindings;
};

struct descriptor_set_layout_t {
    VkDescriptorSetLayout descriptor_set_layout;
    descriptor_set_layout_config_t config;
    operator VkDescriptorSetLayout() {
        return descriptor_set_layout;
    }
};

struct descriptor_set_t {
    VkDescriptorSet descriptor_set;
    descriptor_set_layout_handle_t descriptor_set_layout_handle;
    operator VkDescriptorSet() {
        return descriptor_set;
    }
};

struct buffer_descriptor_info_t {
    buffer_handle_t buffer;
    VkDeviceSize offset;
    VkDeviceSize range; 
};

struct command_bind_pipeline_t {
    pipeline_handle_t handle;
};

struct command_dispatch_t {
    uint32_t x, y, z;
};

struct command_bind_descriptor_sets_t {
    // maybe remove pipeline handle and automatically get it from the current bound pipeline ? 
    pipeline_handle_t pipeline_handle;
    descriptor_set_handle_t p_descriptors[8];  // 8 is widely minimum available for desktops
    uint32_t first_set;
    size_t count;
};

struct command_push_constant_t {
    // maybe remove pipeline handle and automatically get it from the current bound pipeline ? 
    pipeline_handle_t pipeline_handle;
    uint32_t offset;
    uint32_t size;
    VkShaderStageFlags shader_stage;
    char *data[128];  // 128 is the max allowed on most platforms
};

enum class command_type_t {
    e_none = 0,
    e_bind_pipeline,
    e_dispatch,
    e_bind_descriptor_sets,
    e_push_constant,
};

struct command_t {
    command_type_t type = command_type_t::e_none;
    union {
        command_bind_pipeline_t bind_pipeline;
        command_dispatch_t dispatch;
        command_bind_descriptor_sets_t bind_descriptor_sets;
        command_push_constant_t push_constant;
    } as;
};

class command_list_t {
public:
    void bind_pipeline(pipeline_handle_t pipeline_handle) {
        command_t command{ .type = command_type_t::e_bind_pipeline };
        command.as.bind_pipeline.handle = pipeline_handle;

        _commands.push_back(command);
    }   
    void dispatch(uint32_t x, uint32_t y, uint32_t z) {
        command_t command{ .type = command_type_t::e_dispatch };
        command.as.dispatch = { x, y, z };

        _commands.push_back(command);
    }
    template <size_t count>
    void bind_descriptor_sets(pipeline_handle_t pipeline_handle, uint32_t first_set, std::array<descriptor_set_handle_t, count> descriptor_sets) {
        command_t command{ .type = command_type_t::e_bind_descriptor_sets };
        command.as.bind_descriptor_sets.pipeline_handle = pipeline_handle;
        command.as.bind_descriptor_sets.first_set = first_set;
        for (size_t i = 0; i < count; i++) {
            command.as.bind_descriptor_sets.p_descriptors[i] = descriptor_sets[i];
        }
        command.as.bind_descriptor_sets.count = count;

        _commands.push_back(command);
    }

    template <typename type_t> 
    void push_constant(pipeline_handle_t pipeline_handle, uint32_t offset, uint32_t size, VkShaderStageFlags shader_stage, type_t val) {
        // do u need to apply the offset here ?
        static_assert(sizeof(type_t) <= 128);
        command_t command{ .type = command_type_t::e_push_constant };
        command.as.push_constant.pipeline_handle = pipeline_handle;
        command.as.push_constant.offset = offset;
        command.as.push_constant.size = size;
        command.as.push_constant.shader_stage = shader_stage;
        std::memcpy(command.as.push_constant.data, &val, sizeof(val));

        _commands.push_back(command);
    }

    friend class context_t;
private:
    std::vector<command_t> _commands;
};

class context_t {
public:
    context_t(bool validation);
    ~context_t();

    swapchain_handle_t create_swapchain(const core::window_t& window);
    // TODO: add destroy swapchain

    shader_module_handle_t create_shader_module(const std::string& code, shader_type_t shader_type, const std::string& name);
    void destroy_shader_module(shader_module_handle_t handle);

    pipeline_handle_t create_compute_pipeline(const pipeline_config_t& config);
    void destroy_pipeline(pipeline_handle_t handle);

    buffer_handle_t create_buffer(const buffer_config_t& config);
    void destroy_buffer(buffer_handle_t handle);
    void *map_buffer(buffer_handle_t handle);
    void unmap_buffer(buffer_handle_t handle);
    // TODO: add flush and invalidate

    // image_handle_t create_image(const image_config_t& config);

    descriptor_set_layout_handle_t create_descriptor_set_layout(const descriptor_set_layout_config_t& config);
    void destroy_descriptor_set_layout(descriptor_set_layout_handle_t handle);

    descriptor_set_handle_t create_descriptor_set(descriptor_set_layout_handle_t descriptor_set_layout_handle);
    void destroy_descriptor_set(descriptor_set_handle_t handle);

    struct update_descriptor_t {
        template <typename info_t>
        update_descriptor_t& push_write(uint32_t binding, info_t info, uint32_t count = 1) {
            assert(context._descriptor_sets.contains(handle));
            descriptor_set_t& descriptor_set = context._descriptor_sets[handle];
            assert(context._descriptor_set_layouts.contains(descriptor_set.descriptor_set_layout_handle));
            auto& descriptor_set_layout = context._descriptor_set_layouts[descriptor_set.descriptor_set_layout_handle];

            VkWriteDescriptorSet write{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            write.dstBinding = binding;
            write.descriptorCount = count;
            write.dstSet = descriptor_set;

            if (std::is_same<info_t, buffer_descriptor_info_t>::value) {
                buffer_descriptor_info_t& buffer_descriptor_info = info;
                assert(context._buffers.contains(buffer_descriptor_info.buffer));
                buffer_t& buffer = context._buffers[buffer_descriptor_info.buffer];

                auto itr = std::find_if(descriptor_set_layout.config.descriptor_set_layout_bindings.begin(),
                                        descriptor_set_layout.config.descriptor_set_layout_bindings.end(),
                                        [&write](const VkDescriptorSetLayoutBinding& layout_binding) {
                                            return write.dstBinding == layout_binding.binding;
                                        });
                assert(itr != descriptor_set_layout.config.descriptor_set_layout_bindings.end());
                write.descriptorType = itr->descriptorType;

                // cursed shit to make it simple, TODO: add arena allocator
                VkDescriptorBufferInfo *vk_buffer_info = new VkDescriptorBufferInfo;
                vk_buffer_info->buffer = buffer;
                vk_buffer_info->offset = buffer_descriptor_info.offset;
                vk_buffer_info->range = buffer_descriptor_info.range;
                write.pBufferInfo = vk_buffer_info;
            }

            writes.push_back(write);
            return *this;
        }

        void commit() {
            vkUpdateDescriptorSets(context._device, writes.size(), writes.data(), 0, nullptr);
            for (auto& write : writes) {
                if (write.pBufferInfo) delete write.pBufferInfo;
                if (write.pImageInfo) delete write.pImageInfo;
            }
            writes.clear();
        }

        context_t& context;
        descriptor_set_handle_t handle;
        std::vector<VkWriteDescriptorSet> writes;
    };

    update_descriptor_t update_descriptor_set(descriptor_set_handle_t handle);

    void exec_command_list_immediate(const command_list_t& command_list) {
        horizon_profile();
        VkCommandBufferAllocateInfo commandbuffer_allocate_info{};
        commandbuffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandbuffer_allocate_info.commandPool = _command_pool;
        commandbuffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandbuffer_allocate_info.commandBufferCount = 1;

        VkCommandBuffer commandbuffer;
        vk_check(vkAllocateCommandBuffers(_device, &commandbuffer_allocate_info, &commandbuffer), "Failed to allocate command buffer");

        VkCommandBufferBeginInfo commandbuffer_begin_info{};
        commandbuffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        commandbuffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vk_check(vkBeginCommandBuffer(commandbuffer, &commandbuffer_begin_info), "Failed to begin command buffer");

        exec_commands(commandbuffer, command_list._commands);
        
        vk_check(vkEndCommandBuffer(commandbuffer), "Failed to end command buffer");

        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &commandbuffer;

        VkFenceCreateInfo fence_create_info{};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VkFence single_use_command_complete_fence;
        vk_check(vkCreateFence(_device, &fence_create_info, nullptr, &single_use_command_complete_fence), "Failed to create fence");
        vk_check(vkQueueSubmit(_graphics_queue, 1, &submit_info, single_use_command_complete_fence), "Failed to submit to queue");
        vk_check(vkWaitForFences(_device, 1, &single_use_command_complete_fence, VK_TRUE, UINT64_MAX), "Failed to wait for fence");

        vkDestroyFence(_device, single_use_command_complete_fence, nullptr);
        vkFreeCommandBuffers(_device, _command_pool, 1, &commandbuffer);
    }

private:

    void exec_commands(VkCommandBuffer commandbuffer, const std::vector<command_t>& commands);

    void create_instance();
    void create_device();
    void create_vma();
    void create_command_pool();
    void create_descriptor_pool();

private:
    const bool _validation;

    vkb::Instance _instance;
    vkb::PhysicalDevice _physical_device;
    vkb::Device _device;

    queue_t _graphics_queue;
    queue_t _present_queue;

    VmaAllocator _vma_allocator;

    VkCommandPool _command_pool;

    VkDescriptorPool _descriptor_pool;

    std::map<swapchain_handle_t, swapchain_t> _swapchains;
    std::map<buffer_handle_t, buffer_t> _buffers;
    std::map<shader_module_handle_t, shader_module_t> _shader_modules;
    std::map<pipeline_handle_t, pipeline_t> _pipelines;
    std::map<descriptor_set_layout_handle_t, descriptor_set_layout_t> _descriptor_set_layouts;
    std::map<descriptor_set_handle_t, descriptor_set_t> _descriptor_sets;
};

// TODO: for future, slot system available id queue with version encoding
// maybe do 1-1 mapping (rather than creating a handle, just use the vkType as the handle)

} // namespace gfx

#define define_fmt(name) \
template<> \
struct fmt::formatter<name> { \
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) { \
        return ctx.end(); \
    } \
    template <typename FormatContext> \
    auto format(const name& input, FormatContext& ctx) -> decltype(ctx.out()) { \
        return format_to(ctx.out(), \
            "{}", \
            input.val); \
    } \
}

define_fmt(gfx::swapchain_handle_t);
define_fmt(gfx::shader_module_handle_t);
define_fmt(gfx::pipeline_handle_t);
define_fmt(gfx::buffer_handle_t);
define_fmt(gfx::descriptor_set_layout_handle_t);
define_fmt(gfx::descriptor_set_handle_t);

#endif
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
#include <limits>
#include <optional>

#define vk_check(result, msg) \
do {                              \
    if (result != VK_SUCCESS) {   \
        horizon_error(msg);       \
        std::terminate();       \
    }                             \
} while (false)

using handle_t = uint64_t;

#define handle(name) \
struct name { \
    handle_t val; \
    name() = default; \
    name(handle_t val) : val(val) {} \
    constexpr name& operator = (const name& new_val) = default; \
    bool operator<(const name& other) const { return val < other.val; } \
    operator handle_t() { return val; } \
    name& operator++(int) { val++; return *this; } \
    name& operator++() { val++; return *this; } \
    std::ostream& operator<<(std::ostream& o) { o << val; return o; } \
}

namespace core {

class window_t;

} // namespace core

namespace gfx {

constexpr handle_t null_handle = std::numeric_limits<handle_t>::max();

handle(swapchain_handle_t);
handle(shader_module_handle_t);
handle(pipeline_handle_t);
handle(buffer_handle_t);
handle(image_handle_t);
handle(image_view_handle_t);
handle(descriptor_set_layout_handle_t);
handle(descriptor_set_handle_t);
handle(semaphore_handle_t);
handle(fence_handle_t);
handle(command_pool_handle_t);

struct queue_t {
    VkQueue  vk_queue;
    uint32_t vk_index;
    operator VkQueue() {
        return vk_queue;
    }
};

enum class shader_type_t {
    e_vertex,
    e_fragment,
    e_compute,
};

struct shader_module_config_t {
    std::string   code;
    std::string   name;
    shader_type_t type;
};

struct shader_module_t {
    VkShaderModule         vk_module;
    shader_module_config_t config;
    operator VkShaderModule() {
        return vk_module;
    }
};

struct swapchain_config_t {

};

struct swapchain_t {
    VkSurfaceKHR                     vk_surface;
    vkb::Swapchain                   vk_swapchain;
    std::vector<image_view_handle_t> image_views;
    std::vector<image_handle_t>      images;
    swapchain_config_t               config;
    operator vkb::Swapchain() {
        return vk_swapchain;
    }
};

struct pipeline_config_t {
    pipeline_config_t();

    pipeline_config_t& add_shader(shader_module_handle_t handle);
    pipeline_config_t& add_descriptor_set_layout(descriptor_set_layout_handle_t handle);
    pipeline_config_t& add_push_constant(uint32_t vk_size, uint32_t vk_offset, VkShaderStageFlags vk_shader_stage);

    pipeline_config_t& add_color_attachment(VkFormat vk_format, const VkPipelineColorBlendAttachmentState& vk_pipeline_color_blend_state);
    pipeline_config_t& set_depth(VkFormat vk_format, const VkPipelineDepthStencilStateCreateInfo& vk_pipeline_depth_state);

    pipeline_config_t& add_dynamic_state(const VkDynamicState& vk_dynamic_state);

    pipeline_config_t& add_vertex_input_binding_description(uint32_t vk_binding, uint32_t vk_stride, VkVertexInputRate vk_input_rate);
    pipeline_config_t& add_vertex_input_attribute_description(uint32_t vk_binding, uint32_t vk_location, VkFormat vk_format, uint32_t vk_offset);   

    pipeline_config_t& set_pipeline_input_assembly_state(const VkPipelineInputAssemblyStateCreateInfo& vk_pipeline_input_assembly_state);
    pipeline_config_t& set_pipeline_rasterization_state(const VkPipelineRasterizationStateCreateInfo& vk_pipeline_rasterization_state);
    pipeline_config_t& set_pipeline_multisample_state(const VkPipelineMultisampleStateCreateInfo& vk_pipeline_multisample_state);

    std::vector<shader_module_handle_t>              shaders{};
    std::vector<descriptor_set_layout_handle_t>      descriptor_set_layouts{};
    std::vector<VkPushConstantRange>                 vk_push_constant_ranges{};
    std::vector<VkFormat>                            vk_color_formats{};
    VkFormat                                         vk_depth_format{};
    std::vector<VkPipelineColorBlendAttachmentState> vk_pipeline_color_blend_attachment_states{};
    VkPipelineDepthStencilStateCreateInfo            vk_pipeline_depth_stencil_state_create_info{};
    std::vector<VkDynamicState>                      vk_dynamic_states{};
    std::vector<VkVertexInputAttributeDescription>   vk_vertex_input_attribute_descriptions{};
    std::vector<VkVertexInputBindingDescription>     vk_vertex_input_binding_descriptions{};
    VkPipelineInputAssemblyStateCreateInfo           vk_pipeline_input_assembly_state{};
    VkPipelineRasterizationStateCreateInfo           vk_pipeline_rasterization_state{};
    VkPipelineMultisampleStateCreateInfo             vk_pipeline_multisample_state{};
};

VkPipelineColorBlendAttachmentState default_color_blend_attachment();

struct pipeline_t {
    VkPipeline          vk_pipeline;
    pipeline_config_t   config;
    VkPipelineLayout    vk_layout;
    VkPipelineBindPoint vk_bind_point;
    operator VkPipeline() {
        return vk_pipeline;
    }
};

struct buffer_config_t {
    size_t                   size;
    VkBufferUsageFlags       vk_usage_flags;
    VmaAllocationCreateFlags vma_allocation_create_flags;
    VmaMemoryUsage           vma_memory_usage = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO;
};

struct buffer_t {
    VkBuffer        vk_buffer;
    VmaAllocation   vma_allocation;
    buffer_config_t config;
    void           *map = nullptr;
    operator VkBuffer() {
        return vk_buffer;
    }
};

constexpr VkImageViewType vk_auto_image_view_type = static_cast<VkImageViewType>(0x7FFFFFFF - 1);  // -1 cause I dont wanna collide with the max enum enum in the enum definition
constexpr VkFormat        vk_auto_image_format = static_cast<VkFormat>(0x7FFFFFFF - 1);  
constexpr uint32_t        vk_auto_mips = 0x7FFFFFFF;   // this should work I guess ??
constexpr uint32_t        vk_auto_layers = 0x7FFFFFFF;   // this should work I guess ??
constexpr uint32_t        vk_default_base_mip_level = 0;
constexpr uint32_t        vk_default_base_array_layer = 0;

struct image_view_config_t {
    VkImageViewType vk_image_view_type = vk_auto_image_view_type;
    VkFormat        vk_format = vk_auto_image_format;
    uint32_t        vk_base_mip_level = vk_default_base_mip_level;
    uint32_t        vk_mips = vk_auto_mips;  // gets the max level count possible for the image
    uint32_t        vk_base_array_layer = vk_default_base_array_layer;
    uint32_t        vk_layers = vk_auto_layers;
};

struct image_view_t {
    VkImageView         vk_image_view;
    image_view_config_t config;
    image_handle_t      image_handle;
    operator VkImageView() {
        return vk_image_view;
    }
};

static const uint32_t vk_auto_calculate_mip_levels = std::numeric_limits<uint32_t>::max();

struct image_config_t {
    uint32_t                 vk_width, vk_height, vk_depth;
    VkImageType              vk_type;
    VkFormat                 vk_format;
    VkImageUsageFlags        vk_usage;
    VmaAllocationCreateFlags vma_allocation_create_flags;
    VmaMemoryUsage           vma_memory_usage = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO;
    uint32_t                 vk_mips = vk_auto_calculate_mip_levels;
    VkSampleCountFlagBits    vk_sample_count = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
    uint32_t                 vk_array_layers = 1;
    VkImageTiling            vk_tiling = VK_IMAGE_TILING_OPTIMAL;
    VkImageLayout            vk_inital_layout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
};

struct image_t {
    VkImage        vk_image;
    VmaAllocation  vma_allocation;
    image_config_t config;
    void          *map = nullptr;
    bool           from_swapchain = false;
    operator VkImage() {
        return vk_image;
    }
};

struct descriptor_set_layout_config_t {
    descriptor_set_layout_config_t& add_layout_binding(const VkDescriptorSetLayoutBinding& descriptor_set_layout_binding);
    descriptor_set_layout_config_t& add_layout_binding(uint32_t binding, VkDescriptorType descriptor_type, VkShaderStageFlags stage_flags, uint32_t count = 1);
    
    std::vector<VkDescriptorSetLayoutBinding> vk_descriptor_set_layout_bindings;
};

struct descriptor_set_layout_t {
    VkDescriptorSetLayout          vk_descriptor_set_layout;
    descriptor_set_layout_config_t config;
    operator VkDescriptorSetLayout() {
        return vk_descriptor_set_layout;
    }
};

struct descriptor_set_t {
    VkDescriptorSet                vk_descriptor_set;
    descriptor_set_layout_handle_t descriptor_set_layout_handle;
    operator VkDescriptorSet() {
        return vk_descriptor_set;
    }
};

struct buffer_descriptor_info_t {
    buffer_handle_t buffer;
    VkDeviceSize    vk_offset;
    VkDeviceSize    vk_range; 
};

struct semaphore_config_t {

};

struct semaphore_t {
    VkSemaphore        vk_semaphore;
    semaphore_config_t config;
    operator VkSemaphore() {
        return vk_semaphore;
    }
};

struct fence_config_t {

};

struct fence_t {
    VkFence        vk_fence;
    fence_config_t config;
    operator VkFence() {
        return vk_fence;
    }
};

struct command_pool_config_t {

};

struct command_pool_t {
    VkCommandPool         vk_command_pool;
    command_pool_config_t config;
    operator VkCommandPool() {
        return vk_command_pool;
    }
};

struct command_bind_pipeline_t {
    pipeline_handle_t handle;
};

struct command_dispatch_t {
    uint32_t x, y, z;
};

struct command_bind_descriptor_sets_t {
    descriptor_set_handle_t p_descriptors[8];  // 8 is widely minimum available for desktops
    uint32_t                first_set;
    size_t                  count;
};

struct command_push_constant_t {
    uint32_t           offset;
    uint32_t           size;
    VkShaderStageFlags shader_stage;
    char              *data[128];  // 128 is the max allowed on most platforms
};

struct render_attachment_t {
    image_view_handle_t image_view_handle;
    VkImageLayout       image_layout;
    VkAttachmentLoadOp  load_op;
    VkAttachmentStoreOp store_op;
    VkClearValue        clear_value;
};

struct command_begin_rendering_t {
    render_attachment_t p_color_attachments[8];
    size_t              color_attachments_count;
    render_attachment_t depth_attachment;
    bool                use_depth;
    VkRect2D            render_area;
    uint32_t            layer_count;
};

struct command_end_rendering_t {};

struct command_draw_t {
    uint32_t vertex_count; 
    uint32_t instance_count; 
    uint32_t first_vertex; 
    uint32_t first_instance;
};

struct command_image_memory_barrier_t {
    image_handle_t       image;
    VkImageLayout        old_image_layout;
    VkImageLayout        new_image_layout;
    VkAccessFlags        src_access_mask;
    VkAccessFlags        dst_access_mask;
    VkPipelineStageFlags src_pipeline_stage;
    VkPipelineStageFlags dst_pipeline_stage;
};

struct command_set_viewport_and_scissor_t {
    VkViewport viewport;
    VkRect2D   scissor;
};

enum class command_type_t {
    e_none = 0,
    e_bind_pipeline,
    e_dispatch,
    e_bind_descriptor_sets,
    e_push_constant,
    e_begin_rendering,
    e_end_rendering,
    e_draw,
    e_image_memory_barrier,
    e_set_viewport_and_scissor,
};

struct command_t {
    command_type_t type = command_type_t::e_none;
    union {
        command_bind_pipeline_t            bind_pipeline;
        command_dispatch_t                 dispatch;
        command_bind_descriptor_sets_t     bind_descriptor_sets;
        command_push_constant_t            push_constant;
        command_begin_rendering_t          begin_rendering;
        command_end_rendering_t            end_rendering;
        command_draw_t                     draw;
        command_image_memory_barrier_t     image_memory_barrier;
        command_set_viewport_and_scissor_t set_viewport_and_scissor;
    } as;
};

class command_list_t {
public:
    void bind_pipeline(pipeline_handle_t pipeline_handle);
    void dispatch(uint32_t x, uint32_t y, uint32_t z);
    template <size_t count>
    void bind_descriptor_sets(pipeline_handle_t pipeline_handle, uint32_t first_set, std::array<descriptor_set_handle_t, count> descriptor_sets) {
        horizon_profile();
        command_t command{ .type = command_type_t::e_bind_descriptor_sets };
        // command.as.bind_descriptor_sets.pipeline_handle = pipeline_handle;
        command.as.bind_descriptor_sets.first_set = first_set;
        for (size_t i = 0; i < count; i++) {
            command.as.bind_descriptor_sets.p_descriptors[i] = descriptor_sets[i];
        }
        command.as.bind_descriptor_sets.count = count;

        _commands.push_back(command);
    }
    template <typename type_t> 
    void push_constant(pipeline_handle_t pipeline_handle, uint32_t offset, uint32_t size, VkShaderStageFlags shader_stage, type_t val) {
        horizon_profile();
        // do u need to apply the offset here ?
        static_assert(sizeof(type_t) <= 128);
        command_t command{ .type = command_type_t::e_push_constant };
        // command.as.push_constant.pipeline_handle = pipeline_handle;
        command.as.push_constant.offset = offset;
        command.as.push_constant.size = size;
        command.as.push_constant.shader_stage = shader_stage;
        std::memcpy(command.as.push_constant.data, &val, sizeof(val));

        _commands.push_back(command);
    }
    void begin_rendering(const command_begin_rendering_t& command_begin_rendering);
    void end_rendering();
    void draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance);
    void image_memory_barrier(image_handle_t handle, VkImageLayout old_image_layout, VkImageLayout new_image_layout, VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask, VkPipelineStageFlags src_pipeline_stage, VkPipelineStageFlags dst_pipeline_stage);
    void set_viewport_and_scissor(const VkViewport& viewport, const VkRect2D& scissor);

    friend class context_t;
private:
    std::vector<command_t> _commands;
};

class context_t {
public:
    context_t(bool validation);
    ~context_t();

    swapchain_handle_t create_swapchain(const core::window_t& window);
    void destroy_swapchain(swapchain_handle_t handle);
    std::vector<image_handle_t> swapchain_images(swapchain_handle_t handle);
    std::vector<image_view_handle_t> swapchain_image_views(swapchain_handle_t handle);
    std::optional<uint32_t> acquire_swapchain_next_image_index(swapchain_handle_t handle, semaphore_handle_t semaphore_handle, fence_handle_t fence_handle);
    template <size_t count>
    void present_swapchain(std::array<swapchain_handle_t, count> handles, std::array<uint32_t, count> image_indices, std::vector<semaphore_handle_t> semaphore_handles) {
        VkSwapchainKHR *vk_swapchains = reinterpret_cast<VkSwapchainKHR *>(alloca(count * sizeof(VkSwapchainKHR)));
        for (size_t i = 0; i < count; i++) {
            assert(_swapchains.contains(handles[i]));
            swapchain_t& swapchain = _swapchains[handles[i]];
            vk_swapchains[i] = swapchain.vk_swapchain;
        }
        VkSemaphore *vk_semaphores = reinterpret_cast<VkSemaphore *>(alloca(semaphore_handles.size() * sizeof(VkSemaphore)));
        for (size_t i = 0; i < semaphore_handles.size(); i++) {
            assert(_semaphores.contains(semaphore_handles[i]));
            semaphore_t& semaphore = _semaphores[semaphore_handles[i]];
            vk_semaphores[i] = semaphore;
        }

        VkResult *results = reinterpret_cast<VkResult *>(alloca(count * sizeof(VkResult)));

        VkPresentInfoKHR vk_present_info{ .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        vk_present_info.waitSemaphoreCount = semaphore_handles.size();
        vk_present_info.pWaitSemaphores = vk_semaphores;
        vk_present_info.swapchainCount = count;
        vk_present_info.pSwapchains = vk_swapchains;
        vk_present_info.pImageIndices = image_indices.data();
        vk_present_info.pResults = results;

        {
            auto result = vkQueuePresentKHR(_vk_present_queue, &vk_present_info);
            if (result != VK_SUCCESS) {
                horizon_error("Failed to present");
                std::terminate();
            }
        }

        for (size_t i = 0; i < count; i++) {
            if (results[i] != VK_SUCCESS) {
                horizon_error("Failed to present");
                std::terminate();
            }
        }
        
    }

    shader_module_handle_t create_shader_module(const shader_module_config_t& config);
    void destroy_shader_module(shader_module_handle_t handle);

    pipeline_handle_t create_compute_pipeline(const pipeline_config_t& config);
    pipeline_handle_t create_graphics_pipeline(const pipeline_config_t& config);
    void destroy_pipeline(pipeline_handle_t handle);

    buffer_handle_t create_buffer(const buffer_config_t& config);
    void destroy_buffer(buffer_handle_t handle);
    void *map_buffer(buffer_handle_t handle);
    void unmap_buffer(buffer_handle_t handle);
    // TODO: add flush and invalidate

    image_handle_t create_image(const image_config_t& config);
    void destroy_image(image_handle_t handle);
    void *map_image(image_handle_t handle);
    void unmap_image(image_handle_t handle);
    // TODO: add flush and invalidate

    image_view_handle_t create_image_view(image_handle_t image_handle, const image_view_config_t& config);
    void destroy_image_view(image_view_handle_t handle);

    descriptor_set_layout_handle_t create_descriptor_set_layout(const descriptor_set_layout_config_t& config);
    void destroy_descriptor_set_layout(descriptor_set_layout_handle_t handle);

    descriptor_set_handle_t allocate_descriptor_set(descriptor_set_layout_handle_t descriptor_set_layout_handle);
    void free_descriptor_set(descriptor_set_handle_t handle);

    struct update_descriptor_t {
        template <typename info_t>
        update_descriptor_t& push_write(uint32_t binding, info_t info, uint32_t count = 1) {
            horizon_profile();
            assert(context._descriptor_sets.contains(handle));
            descriptor_set_t& descriptor_set = context._descriptor_sets[handle];
            assert(context._descriptor_set_layouts.contains(descriptor_set.descriptor_set_layout_handle));
            auto& descriptor_set_layout = context._descriptor_set_layouts[descriptor_set.descriptor_set_layout_handle];

            VkWriteDescriptorSet vk_write{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            vk_write.dstBinding = binding;
            vk_write.descriptorCount = count;
            vk_write.dstSet = descriptor_set;

            if (std::is_same<info_t, buffer_descriptor_info_t>::value) {
                buffer_descriptor_info_t& buffer_descriptor_info = info;
                assert(context._buffers.contains(buffer_descriptor_info.buffer));
                buffer_t& buffer = context._buffers[buffer_descriptor_info.buffer];

                auto itr = std::find_if(descriptor_set_layout.config.vk_descriptor_set_layout_bindings.begin(),
                                        descriptor_set_layout.config.vk_descriptor_set_layout_bindings.end(),
                                        [&vk_write](const VkDescriptorSetLayoutBinding& layout_binding) {
                                            return vk_write.dstBinding == layout_binding.binding;
                                        });
                assert(itr != descriptor_set_layout.config.vk_descriptor_set_layout_bindings.end());
                vk_write.descriptorType = itr->descriptorType;

                // cursed shit to make it simple, TODO: add arena allocator
                VkDescriptorBufferInfo *vk_buffer_info = new VkDescriptorBufferInfo;
                vk_buffer_info->buffer = buffer;
                vk_buffer_info->offset = buffer_descriptor_info.vk_offset;
                vk_buffer_info->range = buffer_descriptor_info.vk_range;
                vk_write.pBufferInfo = vk_buffer_info;
            }

            vk_writes.push_back(vk_write);
            return *this;
        }

        void commit() {
            horizon_profile();
            vkUpdateDescriptorSets(context._vk_device, vk_writes.size(), vk_writes.data(), 0, nullptr);
            for (auto& vk_write : vk_writes) {
                if (vk_write.pBufferInfo) delete vk_write.pBufferInfo;
                if (vk_write.pImageInfo) delete vk_write.pImageInfo;
            }
            vk_writes.clear();
        }

        context_t& context;
        descriptor_set_handle_t handle;
        std::vector<VkWriteDescriptorSet> vk_writes;
    };

    update_descriptor_t update_descriptor_set(descriptor_set_handle_t handle);

    semaphore_handle_t create_semaphore(const semaphore_config_t& config);
    void destroy_semaphore(semaphore_handle_t handle);

    fence_handle_t create_fence(const fence_config_t& config);
    void destroy_fence(fence_handle_t handle);
    template <size_t count>
    void wait_fence(std::array<fence_handle_t, count> handles, bool wait_all = true) {
        horizon_profile();
        VkFence *vk_fences = reinterpret_cast<VkFence *>(alloca(count * sizeof(VkFence)));
        for (size_t i = 0; i < count; i++) {
            assert(_fences.contains(handles[i]));
            fence_t& fence = _fences[handles[i]];
            vk_fences[i] = fence;
        }
        auto result = vkWaitForFences(_vk_device, count, vk_fences, wait_all, UINT64_MAX);
        if (result != VK_SUCCESS) {
            horizon_error("Failed to wait for fence");
            std::terminate();
        }
    }
    template <size_t count>
    void reset_fence(std::array<fence_handle_t, count> handles) {
        horizon_profile();
        VkFence *vk_fences = reinterpret_cast<VkFence *>(alloca(count * sizeof(VkFence)));
        for (size_t i = 0; i < count; i++) {
            assert(_fences.contains(handles[i]));
            fence_t& fence = _fences[handles[i]];
            vk_fences[i] = fence;
        }
        auto result = vkResetFences(_vk_device, count, vk_fences);
        if (result != VK_SUCCESS) {
            horizon_error("Failed to reset fence");
            std::terminate();
        }
    }

    command_pool_handle_t create_command_pool(const command_pool_config_t& config);
    void destroy_command_pool(command_pool_handle_t handle);
    template <size_t count>
    std::array<VkCommandBuffer, count> allocate_commandbuffers(command_pool_handle_t handle) {
        assert(_command_pools.contains(handle));
        command_pool_t& command_pool = _command_pools[handle];
        
        std::array<VkCommandBuffer, count> vk_commandbuffers;

        VkCommandBufferAllocateInfo vk_commandbuffer_allocate_info{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        vk_commandbuffer_allocate_info.commandBufferCount = count;
        vk_commandbuffer_allocate_info.commandPool = command_pool;
        vk_commandbuffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        auto result = vkAllocateCommandBuffers(_vk_device, &vk_commandbuffer_allocate_info, vk_commandbuffers.data());
        if (result != VK_SUCCESS) {
            horizon_error("Failed to allocate commandbuffers");
            std::terminate();
        }

        return vk_commandbuffers;
    }

    template <size_t count>
    void free_commandbuffers(command_pool_handle_t handle, const std::array<VkCommandBuffer, count> vk_commandbuffers) {
        assert(_command_pools.contains(handle));
        command_pool_t& command_pool = _command_pools[handle];

        vkFreeCommandBuffers(_vk_device, command_pool, count, vk_commandbuffers.data());
    }

    void begin_commandbuffer(VkCommandBuffer vk_commandbuffer, bool single_use = false) {
        VkCommandBufferBeginInfo vk_commandbuffer_begin_info{};
        vk_commandbuffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vk_commandbuffer_begin_info.flags = single_use ? VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT : 0;
        vk_check(vkBeginCommandBuffer(vk_commandbuffer, &vk_commandbuffer_begin_info), "Failed to begin command buffer");
    }

    void end_commandbuffer(VkCommandBuffer vk_commandbuffer) {
        auto result = vkEndCommandBuffer(vk_commandbuffer);
        if (result != VK_SUCCESS) {
            horizon_error("Failed to end commandbuffer");
            std::terminate();
        }
    }

    void submit_commandbuffer(VkCommandBuffer vk_commandbuffer, std::vector<semaphore_handle_t> wait_semaphore_handle, std::vector<VkPipelineStageFlags> pipeline_stages, std::vector<semaphore_handle_t> signal_semaphore_handle, fence_handle_t fence_handle) {
        VkSemaphore *p_vk_wait_semaphore = reinterpret_cast<VkSemaphore *>(alloca(wait_semaphore_handle.size() * sizeof(VkSemaphore)));
        VkSemaphore *p_vk_signal_semaphore = reinterpret_cast<VkSemaphore *>(alloca(signal_semaphore_handle.size() * sizeof(VkSemaphore)));

        for (size_t i = 0; i < wait_semaphore_handle.size(); i++) {
            assert(_semaphores.contains(wait_semaphore_handle[i]));
            semaphore_t& semaphore = _semaphores[wait_semaphore_handle[i]];
            p_vk_wait_semaphore[i] = semaphore;
        }
        for (size_t i = 0; i < signal_semaphore_handle.size(); i++) {
            assert(_semaphores.contains(signal_semaphore_handle[i]));
            semaphore_t& semaphore = _semaphores[signal_semaphore_handle[i]];
            p_vk_signal_semaphore[i] = semaphore;
        }

        assert(_fences.contains(fence_handle));
        fence_t fence = _fences[fence_handle];

        assert(pipeline_stages.size() == wait_semaphore_handle.size());
    	VkPipelineStageFlags *vk_wait_stages = pipeline_stages.data();

        VkSubmitInfo vk_submit_info{ .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO };
        vk_submit_info.waitSemaphoreCount = wait_semaphore_handle.size();
        vk_submit_info.pWaitSemaphores = p_vk_wait_semaphore;
        vk_submit_info.signalSemaphoreCount = signal_semaphore_handle.size();
        vk_submit_info.pSignalSemaphores = p_vk_signal_semaphore;
        vk_submit_info.pWaitDstStageMask = vk_wait_stages;
        vk_submit_info.commandBufferCount = 1;
        vk_submit_info.pCommandBuffers = &vk_commandbuffer;

        auto result = vkQueueSubmit(_vk_graphics_queue, 1, &vk_submit_info, fence);
        if (result != VK_SUCCESS) {
            horizon_error("Failed to submit to queue");
            std::terminate();
        }
    }

    void exec_command_list_immediate(command_pool_handle_t handle, const command_list_t& command_list) {
        horizon_profile();
        assert(_command_pools.contains(handle));
        auto commandbuffers = allocate_commandbuffers<1>(handle);

        begin_commandbuffer(commandbuffers[0], true);
        
        exec_commands(commandbuffers[0], command_list);
        
        end_commandbuffer(commandbuffers[0]);

        auto fence = create_fence({});
        reset_fence<1>({fence});
        submit_commandbuffer(commandbuffers[0], {}, {}, {}, fence);
        wait_fence<1>({fence});
        destroy_fence(fence);
        free_commandbuffers<1>(handle, commandbuffers);
    }

    void exec_commands(VkCommandBuffer commandbuffer, const command_list_t& command_list);

private:


    void create_instance();
    void create_device();
    void create_vma();
    // void create_command_pool();
    void create_descriptor_pool();

private:
    const bool                                                        _validation;
    vkb::Instance                                                     _vk_instance;
    vkb::PhysicalDevice                                               _vk_physical_device;
    vkb::Device                                                       _vk_device;
    queue_t                                                           _vk_graphics_queue;
    queue_t                                                           _vk_present_queue;
    VmaAllocator                                                      _vma_allocator;
    VkDescriptorPool                                                  _vk_descriptor_pool;
    std::map<swapchain_handle_t, swapchain_t>                         _swapchains;
    std::map<buffer_handle_t, buffer_t>                               _buffers;
    std::map<image_handle_t, image_t>                                 _images;
    std::map<image_view_handle_t, image_view_t>                       _image_views;
    std::map<shader_module_handle_t, shader_module_t>                 _shader_modules;
    std::map<pipeline_handle_t, pipeline_t>                           _pipelines;
    std::map<descriptor_set_layout_handle_t, descriptor_set_layout_t> _descriptor_set_layouts;
    std::map<descriptor_set_handle_t, descriptor_set_t>               _descriptor_sets;
    std::map<semaphore_handle_t, semaphore_t>                         _semaphores;
    std::map<fence_handle_t, fence_t>                                 _fences;
    std::map<command_pool_handle_t, command_pool_t>                   _command_pools;
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
define_fmt(gfx::image_handle_t);
define_fmt(gfx::image_view_handle_t);
define_fmt(gfx::descriptor_set_layout_handle_t);
define_fmt(gfx::descriptor_set_handle_t);
define_fmt(gfx::semaphore_handle_t);
define_fmt(gfx::fence_handle_t);
define_fmt(gfx::command_pool_handle_t);

#endif
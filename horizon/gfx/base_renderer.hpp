#ifndef GFX_BASE_RENDERER_HPP
#define GFX_BASE_RENDERER_HPP

#include "core/window.hpp"
#include "context.hpp"
#include "helper.hpp"

#include <vector>

namespace renderer {

enum class resource_policy_t {
    e_sparse,
    e_every_frame,
    // e_ping_pong,
};

define_handle(handle_buffer_t);
define_handle(handle_descriptor_set_t);

namespace internal {

struct buffer_t {
    std::vector<gfx::handle_buffer_t> handle_buffers;
    resource_policy_t policy;
};

struct descriptor_set_t {
    std::vector<gfx::handle_descriptor_set_t> handle_descriptor_sets;
    resource_policy_t policy;
};

} // namespace internal

struct base_renderer_t;

struct buffer_descriptor_info_t {
    handle_buffer_t handle_buffer = null_handle;
    VkDeviceSize    vk_offset = 0;
    VkDeviceSize    vk_range = VK_WHOLE_SIZE;
};

using image_descriptor_info_t = gfx::image_descriptor_info_t;

enum class descriptor_info_type_t {
    e_buffer,
    e_image,
};

struct descriptor_info_t {
    uint32_t binding;
    uint32_t count;
    descriptor_info_type_t type;
    union {
        buffer_descriptor_info_t buffer_info;
        image_descriptor_info_t image_info;
    } as;
};

struct update_descriptor_set_t {
    update_descriptor_set_t& push_buffer_write(uint32_t binding, const buffer_descriptor_info_t& info, uint32_t count = 1);
    update_descriptor_set_t& push_image_write(uint32_t binding, const image_descriptor_info_t& info, uint32_t count = 1);
    void commit();

    base_renderer_t& renderer;
    handle_descriptor_set_t handle = null_handle;
    std::vector<descriptor_info_t> infos;
};

struct base_renderer_t {
    constexpr static size_t MAX_FRAMES_IN_FLIGHT = 2;
    
    base_renderer_t(const core::window_t& window, gfx::context_t& context, gfx::handle_sampler_t sampler, gfx::handle_image_view_t final_image_view);
    ~base_renderer_t();

    void begin(bool transition_swapchain_image = true);
    void end();

    handle_buffer_t create_buffer(resource_policy_t policy, const gfx::config_buffer_t& config);
    gfx::handle_buffer_t buffer(handle_buffer_t handle);

    handle_descriptor_set_t allocate_descriptor_set(resource_policy_t policy, const gfx::config_descriptor_set_t& config);
    gfx::handle_descriptor_set_t descriptor_set(handle_descriptor_set_t handle);
    update_descriptor_set_t update_descriptor_set(handle_descriptor_set_t handle);

    gfx::handle_commandbuffer_t current_commandbuffer();
    gfx::handle_image_t current_swapchain_image();
    gfx::handle_image_view_t current_swapchain_image_view();
    gfx::rendering_attachment_t swapchain_rendering_attachment(VkClearValue vk_clear_value, VkImageLayout vk_layout, VkAttachmentLoadOp vk_load_op, VkAttachmentStoreOp vk_store_op);

    const core::window_t& window;
    gfx::context_t&       context;
    gfx::handle_swapchain_t     swapchain;
    gfx::handle_command_pool_t  command_pool;
    gfx::handle_commandbuffer_t commandbuffers[MAX_FRAMES_IN_FLIGHT];
    gfx::handle_fence_t         in_flight_fences[MAX_FRAMES_IN_FLIGHT];
    gfx::handle_semaphore_t     image_available_semaphores[MAX_FRAMES_IN_FLIGHT];
    gfx::handle_semaphore_t     render_finished_semaphores[MAX_FRAMES_IN_FLIGHT];

    uint32_t current_frame = 0;
    uint32_t next_image = 0;

    std::map<handle_buffer_t, internal::buffer_t> _buffers;
    std::map<handle_descriptor_set_t, internal::descriptor_set_t> _descriptor_sets;

    gfx::handle_pipeline_t swapchain_pipeline;
    gfx::handle_descriptor_set_t swapchain_descriptor_set;
};

template <typename type_t>
handle_buffer_t create_and_push_vector(base_renderer_t& renderer, VkBufferUsageFlags vk_buffer_usage_flags, VmaAllocationCreateFlags vma_allocation_create_flags, const std::vector<type_t>& data) {
    gfx::config_buffer_t config_buffer{};
    config_buffer.vk_size = data.size() * sizeof(type_t);
    config_buffer.vk_buffer_usage_flags = vk_buffer_usage_flags | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    config_buffer.vma_allocation_create_flags = vma_allocation_create_flags;
    handle_buffer_t buffer = renderer.create_buffer(resource_policy_t::e_sparse, config_buffer);

    config_buffer.vk_buffer_usage_flags = 0;
    config_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    config_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    gfx::handle_buffer_t staging_buffer = renderer.context.create_buffer(config_buffer);

    std::memcpy(renderer.context.map_buffer(staging_buffer), data.data(), config_buffer.vk_size);

    gfx::handle_commandbuffer_t commandbuffer = gfx::helper::start_single_use_commandbuffer(renderer.context, renderer.command_pool);
    renderer.context.cmd_copy_buffer(commandbuffer, staging_buffer, renderer.buffer(buffer), { .vk_size = config_buffer.vk_size });
    gfx::helper::end_single_use_commandbuffer(renderer.context, commandbuffer);

    renderer.context.destroy_buffer(staging_buffer);

    return buffer;
}

} // namespace renderer

#endif
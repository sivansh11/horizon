#include "bindless_manager.hpp"

#define MAX_SLOTS 1000

namespace gfx {

bindless_manager_t::bindless_manager_t(gfx::context_t& context) : _context(context) {
    gfx::config_descriptor_set_layout_t cdsl{};
    cdsl.add_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL);
    cdsl.add_layout_binding(1, VK_DESCRIPTOR_TYPE_SAMPLER       , VK_SHADER_STAGE_ALL, MAX_SLOTS);
    cdsl.add_layout_binding(2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE , VK_SHADER_STAGE_ALL, MAX_SLOTS);
    _dsl = _context.create_descriptor_set_layout(cdsl);
    _ds = _context.allocate_descriptor_set({ .handle_descriptor_set_layout = _dsl });

    gfx::config_buffer_t cb{};
    cb.vk_size = MAX_SLOTS * sizeof(VkDeviceAddress);
    cb.vk_buffer_usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    cb.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    _b = _context.create_buffer(cb);

    _context.update_descriptor_set(_ds)
        .push_buffer_write(0, { .handle_buffer = _b })
        .commit();
}

void bindless_manager_t::slot(gfx::handle_buffer_t handle, uint32_t slot_id) {
    assert(slot_id < MAX_SLOTS);
    _pointers[slot_id] = _context.get_buffer_device_address(handle);
}

handle_sampler_slot_t bindless_manager_t::slot(gfx::handle_sampler_t handle) {
    handle_sampler_slot_t slot_id = _sampler_slot_counter++;
    _sampler_writes.push_back(std::pair{ slot_id, gfx::image_descriptor_info_t{ .handle_sampler = handle } });
    return slot_id;
}

handle_sampled_image_slot_t bindless_manager_t::slot(handle_image_view_t handle, VkImageLayout vk_image_layout) {
    handle_sampled_image_slot_t slot_id = _sampled_image_slot_counter++;
    _sampled_image_writes.push_back(std::pair{ slot_id, gfx::image_descriptor_info_t{ .handle_image_view = handle, .vk_image_layout = vk_image_layout } });
    return slot_id;
}

void bindless_manager_t::flush_updates() {
    VkDeviceAddress *pointers = reinterpret_cast<VkDeviceAddress *>(_context.map_buffer(_b));
    for (uint32_t i = 0; i < MAX_SLOTS; i++) {
        pointers[i] = _pointers[i];
    }

    auto updater = _context.update_descriptor_set(_ds);

    for (auto& [id, sampler_write] : _sampler_writes) {
        updater.push_image_write(1, sampler_write, id);
    }

    for (auto& [id, image_write] : _sampled_image_writes) {
        updater.push_image_write(2, image_write, id);
    }

    updater.commit();
}


} // namespace gfx

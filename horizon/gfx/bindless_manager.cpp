#include "bindless_manager.hpp"

#define MAX_SLOTS 1000

namespace gfx {

bindless_manager_t::bindless_manager_t(gfx::context_t& context) : _context(context) {
    gfx::config_descriptor_set_layout_t cdsl{};
    cdsl.add_layout_binding(0, VK_DESCRIPTOR_TYPE_SAMPLER       , VK_SHADER_STAGE_ALL, MAX_SLOTS);
    cdsl.add_layout_binding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE , VK_SHADER_STAGE_ALL, MAX_SLOTS);
    _dsl = _context.create_descriptor_set_layout(cdsl);
    _ds = _context.allocate_descriptor_set({ .handle_descriptor_set_layout = _dsl });

    gfx::config_pipeline_layout_t cpl{};
    cpl.add_descriptor_set_layout(_dsl);
    _pl = _context.create_pipeline_layout(cpl);
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
    auto updater = _context.update_descriptor_set(_ds);

    for (auto& [id, sampler_write] : _sampler_writes) {
        updater.push_image_write(0, sampler_write, id);
    }

    for (auto& [id, image_write] : _sampled_image_writes) {
        updater.push_image_write(1, image_write, id);
    }

    updater.commit();
}


} // namespace gfx

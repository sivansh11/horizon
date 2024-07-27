#ifndef GFX_BINDLESS_MANAGER_HPP
#define GFX_BINDLESS_MANAGER_HPP

#include "gfx/context.hpp"

#define MAX_SLOTS 1000

namespace gfx {

define_handle(handle_buffer_slot_t);
define_handle(handle_sampler_slot_t);
define_handle(handle_sampled_image_slot_t);

class bindless_manager_t {
public:
    bindless_manager_t(context_t& context);

    void slot(handle_buffer_t handle, uint32_t slot_id);
    handle_sampler_slot_t slot(handle_sampler_t handle);
    handle_sampled_image_slot_t slot(handle_image_view_t handle, VkImageLayout vk_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    void flush_updates();

    handle_descriptor_set_layout_t dsl() { return _dsl; }
    handle_descriptor_set_t ds() { return _ds; }
    handle_pipeline_layout_t pl() { return _pl; }

private:
    context_t&                                                  _context;
    handle_descriptor_set_layout_t                              _dsl;
    handle_descriptor_set_t                                     _ds;
    handle_pipeline_layout_t                                    _pl;

    handle_buffer_t                                             _b;
    std::array<VkDeviceAddress, MAX_SLOTS>                      _pointers;

    std::vector<std::pair<uint32_t, image_descriptor_info_t>>   _sampler_writes;
    uint64_t                                                    _sampler_slot_counter{ 0 };

    std::vector<std::pair<uint32_t, image_descriptor_info_t>>   _sampled_image_writes;
    uint64_t                                                    _sampled_image_slot_counter{ 0 };
};

} // namespace gfx

#endif
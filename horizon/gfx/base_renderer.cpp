#include "base_renderer.hpp"

namespace renderer {

update_descriptor_set_t& update_descriptor_set_t::push_buffer_write(uint32_t binding, const buffer_descriptor_info_t& info, uint32_t count) {
    horizon_profile();
    internal::descriptor_set_t& descriptor_set = utils::assert_and_get_data<internal::descriptor_set_t>(handle, renderer._descriptor_sets);
    internal::buffer_t& buffer = utils::assert_and_get_data<internal::buffer_t>(info.handle_buffer, renderer._buffers);
    if (descriptor_set.policy == resource_policy_t::e_sparse) {
        check(buffer.policy == resource_policy_t::e_sparse, "a sparse descriptor must only point to a sparse buffer");
    }

    descriptor_info_t descriptor_info{};
    descriptor_info.binding = binding;
    descriptor_info.count = count;
    descriptor_info.type = descriptor_info_type_t::e_buffer;
    descriptor_info.as.buffer_info = info;
    infos.push_back(descriptor_info);
    return *this;
}

update_descriptor_set_t& update_descriptor_set_t::push_image_write(uint32_t binding, const image_descriptor_info_t& info, uint32_t count) {
    horizon_profile();
    descriptor_info_t descriptor_info{};
    descriptor_info.binding = binding;
    descriptor_info.count = count;
    descriptor_info.type = descriptor_info_type_t::e_image;
    descriptor_info.as.image_info = info;
    infos.push_back(descriptor_info);
    return *this;
}

void update_descriptor_set_t::commit() {
    horizon_profile();
    internal::descriptor_set_t& descriptor_set = utils::assert_and_get_data<internal::descriptor_set_t>(handle, renderer._descriptor_sets);
    if (descriptor_set.policy == resource_policy_t::e_sparse) {
        // single buffer/image
        auto update_descriptor_set = renderer.context.update_descriptor_set(descriptor_set.handle_descriptor_sets[0]);
        for (auto& info : infos) {
            if (info.type == descriptor_info_type_t::e_buffer) {
                internal::buffer_t& buffer = utils::assert_and_get_data<internal::buffer_t>(info.as.buffer_info.handle_buffer, renderer._buffers);
                gfx::buffer_descriptor_info_t gfx_info{};
                gfx_info.handle_buffer = buffer.handle_buffers[0];
                gfx_info.vk_offset = info.as.buffer_info.vk_offset;
                gfx_info.vk_range = info.as.buffer_info.vk_range;
                update_descriptor_set.push_buffer_write(info.binding, gfx_info, info.count);
            } else if (info.type == descriptor_info_type_t::e_image) {
                gfx::image_descriptor_info_t gfx_info{};
                gfx_info = info.as.image_info;
                update_descriptor_set.push_image_write(info.binding, gfx_info, info.count);
            }
        }
        update_descriptor_set.commit();
    } else if (descriptor_set.policy == resource_policy_t::e_every_frame) {
        // single image
        // single/multiple buffer
        for (size_t i = 0; i < renderer.MAX_FRAMES_IN_FLIGHT; i++) {
            auto update_descriptor_set = renderer.context.update_descriptor_set(descriptor_set.handle_descriptor_sets[i]);
            for (auto& info : infos) {
                if (info.type == descriptor_info_type_t::e_buffer) {
                    internal::buffer_t& buffer = utils::assert_and_get_data<internal::buffer_t>(info.as.buffer_info.handle_buffer, renderer._buffers);
                    if (buffer.policy == resource_policy_t::e_sparse) {
                        gfx::buffer_descriptor_info_t gfx_info{};
                        gfx_info.handle_buffer = buffer.handle_buffers[0];
                        gfx_info.vk_offset = info.as.buffer_info.vk_offset;
                        gfx_info.vk_range = info.as.buffer_info.vk_range;
                        update_descriptor_set.push_buffer_write(info.binding, gfx_info, info.count);
                    } else if (buffer.policy == resource_policy_t::e_every_frame) {
                        gfx::buffer_descriptor_info_t gfx_info{};
                        gfx_info.handle_buffer = buffer.handle_buffers[i];
                        gfx_info.vk_offset = info.as.buffer_info.vk_offset;
                        gfx_info.vk_range = info.as.buffer_info.vk_range;
                        update_descriptor_set.push_buffer_write(info.binding, gfx_info, info.count);
                    }
                } else if (info.type == descriptor_info_type_t::e_image) {
                    gfx::image_descriptor_info_t gfx_info{};
                    gfx_info = info.as.image_info;
                    update_descriptor_set.push_image_write(info.binding, gfx_info, info.count);
                }
            }
            update_descriptor_set.commit();
        }
        // TODO: add images
    }
}

base_renderer_t::base_renderer_t(const core::window_t& window) : window(window), context(true) {
    horizon_profile();
    swapchain = context.create_swapchain(window);
    command_pool = context.create_command_pool({});
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        commandbuffers[i] = context.allocate_commandbuffer({ .handle_command_pool = command_pool });
    }
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        in_flight_fences[i] = context.create_fence({});
    }
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        image_available_semaphores[i] = context.create_semaphore({});
    }
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        render_finished_semaphores[i] = context.create_semaphore({});
    }
}

base_renderer_t::~base_renderer_t() {
    context.wait_idle();
    for (auto& [handle, buffer] : _buffers) {
        for (auto handle_buffer : buffer.handle_buffers) {
            context.destroy_buffer(handle_buffer);
        }
    }
    for (auto& [handle, descriptor_set] : _descriptor_sets) {
        for (auto handle_descriptor_set : descriptor_set.handle_descriptor_sets) {
            context.free_descriptor_set(handle_descriptor_set);
        }
    }
}

void base_renderer_t::begin(bool transition_swapchain_image) {
    horizon_profile();
    gfx::handle_commandbuffer_t commandbuffer = commandbuffers[current_frame];
    gfx::handle_fence_t in_flight_fence = in_flight_fences[current_frame];
    gfx::handle_semaphore_t image_available_semaphore = image_available_semaphores[current_frame];
    gfx::handle_semaphore_t render_finished_semaphore = render_finished_semaphores[current_frame];
    context.wait_fence(in_flight_fence);
    auto swapchain_image = context.get_swapchain_next_image_index(swapchain, image_available_semaphore, null_handle);
    check(swapchain_image, "Failed to get next image");
    next_image = *swapchain_image;
    context.reset_fence(in_flight_fence);
    context.begin_commandbuffer(commandbuffer);
    if (transition_swapchain_image)
        context.cmd_image_memory_barrier(commandbuffer, 
                                         context.get_swapchain_images(swapchain)[next_image],
                                         VK_IMAGE_LAYOUT_UNDEFINED, 
                                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
                                         0, 
                                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 
                                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
}

void base_renderer_t::end() {
    horizon_profile();
    gfx::handle_commandbuffer_t commandbuffer = commandbuffers[current_frame];
    gfx::handle_fence_t in_flight_fence = in_flight_fences[current_frame];
    gfx::handle_semaphore_t image_available_semaphore = image_available_semaphores[current_frame];
    gfx::handle_semaphore_t render_finished_semaphore = render_finished_semaphores[current_frame];
    context.cmd_image_memory_barrier(commandbuffer, 
                                     context.get_swapchain_images(swapchain)[next_image],
                                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
                                     VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 
                                     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 
                                     0, 
                                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                     VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    context.end_commandbuffer(commandbuffer);
    context.submit_commandbuffer(commandbuffer, {image_available_semaphore}, {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}, {render_finished_semaphore}, in_flight_fence);
    context.present_swapchain(swapchain, next_image, {render_finished_semaphore});
    current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

handle_buffer_t base_renderer_t::create_buffer(resource_policy_t policy, const gfx::config_buffer_t& config) {
    horizon_profile();
    internal::buffer_t buffer{ .policy = policy };
    if (policy == resource_policy_t::e_sparse) {
        buffer.handle_buffers.push_back(context.create_buffer(config));
    } else if (buffer.policy == resource_policy_t::e_every_frame) {
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            buffer.handle_buffers.push_back(context.create_buffer(config));
        }
    }
    handle_buffer_t handle = utils::create_and_insert_new_handle<handle_buffer_t>(_buffers, buffer);
    horizon_trace("created buffer");
    return handle;
}

gfx::handle_buffer_t base_renderer_t::buffer(handle_buffer_t handle) {
    horizon_profile();
    internal::buffer_t& buffer = utils::assert_and_get_data<internal::buffer_t>(handle, _buffers);
    if (buffer.policy == resource_policy_t::e_sparse) {
        return buffer.handle_buffers[0];
    } else if (buffer.policy == resource_policy_t::e_every_frame) {
        return buffer.handle_buffers[current_frame];
    }
    check(false, "reached unreachable");
}

handle_descriptor_set_t base_renderer_t::allocate_descriptor_set(resource_policy_t policy, const gfx::config_descriptor_set_t& config) {
    horizon_profile();
    internal::descriptor_set_t descriptor_set{ .policy = policy };
    if (policy == resource_policy_t::e_sparse) {
        descriptor_set.handle_descriptor_sets.push_back(context.allocate_descriptor_set(config));
    } else if (descriptor_set.policy == resource_policy_t::e_every_frame) {
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            descriptor_set.handle_descriptor_sets.push_back(context.allocate_descriptor_set(config));
        }
    }
    handle_descriptor_set_t handle = utils::create_and_insert_new_handle<handle_descriptor_set_t>(_descriptor_sets, descriptor_set);
    horizon_trace("created descriptor set");
    return handle;
}

gfx::handle_descriptor_set_t base_renderer_t::descriptor_set(handle_descriptor_set_t handle) {
    horizon_profile();
    internal::descriptor_set_t& descriptor_set = utils::assert_and_get_data<internal::descriptor_set_t>(handle, _descriptor_sets);
    if (descriptor_set.policy == resource_policy_t::e_sparse) {
        return descriptor_set.handle_descriptor_sets[0];
    } else if (descriptor_set.policy == resource_policy_t::e_every_frame) {
        return descriptor_set.handle_descriptor_sets[current_frame];
    }
    check(false, "reached unreachable");
}

update_descriptor_set_t base_renderer_t::update_descriptor_set(handle_descriptor_set_t handle) {
    horizon_profile();
    return { *this, handle };
}

gfx::handle_commandbuffer_t base_renderer_t::current_commandbuffer() {
    horizon_profile();
    return commandbuffers[current_frame];
}

gfx::handle_image_t base_renderer_t::current_swapchain_image() {
    horizon_profile();
    return context.get_swapchain_images(swapchain)[next_image];
}

gfx::handle_image_view_t base_renderer_t::current_swapchain_image_view() {
    horizon_profile();
    return context.get_swapchain_image_views(swapchain)[next_image];
}

gfx::rendering_attachment_t base_renderer_t::swapchain_rendering_attachment(VkClearValue vk_clear_value, VkImageLayout vk_layout, VkAttachmentLoadOp vk_load_op, VkAttachmentStoreOp vk_store_op) {
    horizon_profile();
    gfx::rendering_attachment_t rendering_attachment{};
    rendering_attachment.clear_value = vk_clear_value;
    rendering_attachment.image_layout = vk_layout;
    rendering_attachment.load_op = vk_load_op;
    rendering_attachment.store_op = vk_store_op;
    rendering_attachment.handle_image_view = context.get_swapchain_image_views(swapchain)[next_image];
    return rendering_attachment;
}

} // namespace renderer

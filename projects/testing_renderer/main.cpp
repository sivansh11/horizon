#include "test.hpp"

#define horizon_profile_enable
#include "core/core.hpp"
#include "core/window.hpp"

#include "gfx/context.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

static const char *test_vertex = R"(
#version 450

layout(location = 0) out vec3 fragColor;

vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}
)";

static const char *test_fragment = R"(
#version 450

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform somthing_t {
    float col_mul;
};

void main() {
    outColor = vec4(fragColor, 1.0) * col_mul;
}
)";

enum class resource_usage_t {
    e_update_each_frame,
    e_update_once,
};

define_handle(handle_buffer_t);
define_handle(handle_descriptor_set_t);
// define_handle(handle_image_t);

namespace internal {

struct buffer_t {
    std::vector<gfx::handle_buffer_t> handle_buffers;
    resource_usage_t resource_usage;
};

struct descriptor_set_t {
    std::vector<gfx::handle_descriptor_set_t> handle_descriptor_sets;
    resource_usage_t resource_usage;
};

// struct image_t {
//     gfx::handle_image_t handle_image;
//     gfx::handle_descriptor_set_t handle_descriptor_set;
// };

} // namespace internal

struct buffer_descriptor_info_t {
    handle_buffer_t handle_buffer;
    VkDeviceSize    vk_offset;
    VkDeviceSize    vk_range;
};

using image_descriptor_info_t = gfx::image_descriptor_info_t;

template <size_t MAX_FRAMES_IN_FLIGHT>
struct update_descriptor_set_t;

template <size_t MAX_FRAMES_IN_FLIGHT>
struct renderer_t {
    renderer_t(const core::window_t& window) : window(window), context(true) {
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

    void begin() {
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
        context.cmd_image_memory_barrier(commandbuffer, 
                                         context.get_swapchain_images(swapchain)[next_image],
                                         VK_IMAGE_LAYOUT_UNDEFINED, 
                                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
                                         0, 
                                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 
                                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    }

    void end() {
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

    gfx::rendering_attachment_t get_swapchain_rendering_attachment(VkClearValue vk_clear_value, VkImageLayout vk_layout, VkAttachmentLoadOp vk_load_op, VkAttachmentStoreOp vk_store_op) {
        gfx::rendering_attachment_t rendering_attachment{};
        rendering_attachment.clear_value = vk_clear_value;
        rendering_attachment.image_layout = vk_layout;
        rendering_attachment.load_op = vk_load_op;
        rendering_attachment.store_op = vk_store_op;
        rendering_attachment.handle_image_view = context.get_swapchain_image_views(swapchain)[next_image];
        return rendering_attachment;
    }

    void cmd_bind_pipeliine(gfx::handle_pipeline_t handle_pipeline) {
        horizon_profile();
        gfx::handle_commandbuffer_t handle_commandbuffer = commandbuffers[current_frame];
        context.cmd_bind_pipeliine(handle_commandbuffer, handle_pipeline);
    }

    void cmd_bind_descriptor_sets(gfx::handle_pipeline_t handle_pipeline, uint32_t vk_first_set, const std::vector<gfx::handle_descriptor_set_t>& handle_descriptor_sets) {
        horizon_profile();
        gfx::handle_commandbuffer_t handle_commandbuffer = commandbuffers[current_frame];
        context.cmd_bind_descriptor_sets(handle_commandbuffer, handle_pipeline, vk_first_set, handle_descriptor_sets);
    }

    void cmd_push_constants(gfx::handle_pipeline_layout_t handle_pipeline_layout, VkShaderStageFlags vk_shader_stages, uint32_t vk_offset, uint32_t vk_size, const void *vk_data) {
        horizon_profile();
        gfx::handle_commandbuffer_t handle_commandbuffer = commandbuffers[current_frame];
        context.cmd_push_constants(handle_commandbuffer, handle_pipeline_layout, vk_shader_stages, vk_offset, vk_size, vk_data);
    }

    void cmd_dispatch(uint32_t vk_group_count_x, uint32_t vk_group_count_y, uint32_t vk_group_count_z) {
        horizon_profile();
        gfx::handle_commandbuffer_t handle_commandbuffer = commandbuffers[current_frame];
        context.cmd_dispatch(handle_commandbuffer, vk_group_count_x, vk_group_count_y, vk_group_count_z);
    }

    void cmd_set_viewport_and_scissor(VkViewport vk_viewport, VkRect2D vk_scissor) {
        horizon_profile();
        gfx::handle_commandbuffer_t handle_commandbuffer = commandbuffers[current_frame];
        context.cmd_set_viewport_and_scissor(handle_commandbuffer, vk_viewport, vk_scissor);
    }

    void cmd_begin_rendering(const std::vector<gfx::rendering_attachment_t>& color_rendering_attachments, const std::optional<gfx::rendering_attachment_t>& depth_rendering_attachment, const VkRect2D& vk_render_area, uint32_t vk_layer_count = 1) {
        horizon_profile();
        gfx::handle_commandbuffer_t handle_commandbuffer = commandbuffers[current_frame];
        context.cmd_begin_rendering(handle_commandbuffer, color_rendering_attachments, depth_rendering_attachment, vk_render_area, vk_layer_count);
    }

    void cmd_end_rendering() {
        horizon_profile();
        gfx::handle_commandbuffer_t handle_commandbuffer = commandbuffers[current_frame];
        context.cmd_end_rendering(handle_commandbuffer);
    }

    void cmd_draw(uint32_t vk_vertex_count, uint32_t vk_instance_count, uint32_t vk_first_vertex, uint32_t vk_first_instance) {
        horizon_profile();
        gfx::handle_commandbuffer_t handle_commandbuffer = commandbuffers[current_frame];
        context.cmd_draw(handle_commandbuffer, vk_vertex_count, vk_instance_count, vk_first_vertex, vk_first_instance);
    }

    void cmd_image_memory_barrier(gfx::handle_image_t handle_image, VkImageLayout vk_old_image_layout, VkImageLayout vk_new_image_layout, VkAccessFlags vk_src_access_mask, VkAccessFlags vk_dst_access_mask, VkPipelineStageFlags vk_src_pipeline_stage, VkPipelineStageFlags vk_dst_pipeline_stage) {
        horizon_profile();
        gfx::handle_commandbuffer_t handle_commandbuffer = commandbuffers[current_frame];
        context.cmd_image_memory_barrier(handle_commandbuffer, handle_image, vk_old_image_layout, vk_new_image_layout, vk_src_access_mask, vk_dst_access_mask, vk_src_pipeline_stage, vk_dst_pipeline_stage);
    }

    handle_buffer_t create_buffer(resource_usage_t usage, const gfx::config_buffer_t& config) {
        horizon_profile();
        internal::buffer_t buffer{ .resource_usage = usage };
        if (usage == resource_usage_t::e_update_once) {
            buffer.handle_buffers.push_back(context.create_buffer(config));
        } else {
            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                buffer.handle_buffers.push_back(context.create_buffer(config));
            }
        }
        handle_buffer_t handle = utils::create_and_insert_new_handle<handle_buffer_t>(_buffers, buffer);
        horizon_trace("created buffer");
        return handle;
    }

    handle_descriptor_set_t allocate_descriptor_set(resource_usage_t usage, gfx::handle_descriptor_set_layout_t handle_descriptor_set_layout) {
        horizon_profile();
        internal::descriptor_set_t descriptor_set{ .resource_usage = usage };
        if (usage == resource_usage_t::e_update_once) {
            descriptor_set.handle_descriptor_sets.push_back(context.allocate_descriptor_set({.handle_descriptor_set_layout = handle_descriptor_set_layout}));
        } else {
            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                descriptor_set.handle_descriptor_sets.push_back(context.allocate_descriptor_set({.handle_descriptor_set_layout = handle_descriptor_set_layout}));
            }
        }
        handle_descriptor_set_t handle = utils::create_and_insert_new_handle<handle_descriptor_set_t>(_descriptor_sets, descriptor_set);
        horizon_trace("created descriptor set");
        return handle;
    }

    gfx::handle_buffer_t buffer(handle_buffer_t handle) {
        internal::buffer_t& buffer = utils::assert_and_get_data<internal::buffer_t>(handle, _buffers);
        if (buffer.resource_usage == resource_usage_t::e_update_once) {
            return buffer.handle_buffers[0];
        } else {
            return buffer.handle_buffers[current_frame];
        }
    }

    gfx::handle_descriptor_set_t descriptor_set_descriptor_set(handle_descriptor_set_t handle) {
        internal::descriptor_set_t& descriptor_set = utils::assert_and_get_data<internal::descriptor_set_t>(handle, _descriptor_sets);
        if (descriptor_set.resource_usage == resource_usage_t::e_update_once) {
            return descriptor_set.handle_descriptor_sets[0];
        } else {
            return descriptor_set.handle_descriptor_sets[current_frame];
        }
    }

    

    // void update_buffer_descriptor_set(handle_buffer_t handle, uint32_t binding, buffer_descriptor_info_t info, uint32_t count = 1) {
    //     internal::buffer_t& buffer = utils::assert_and_get_data<internal::buffer_t>(handle, _buffers);
    //     for (size_t i = 0; i < buffer.handle_buffers.size(); i++) {
    //         gfx::buffer_descriptor_info_t gfx_info{};
    //         gfx_info.vk_offset = info.vk_offset;
    //         gfx_info.vk_range = info.vk_range;
    //         gfx_info.handle_buffer = buffer.handle_buffers[i];
    //         context.update_descriptor_set(buffer.handle_descriptor_sets[i]).push_buffer_write(binding, gfx_info, count).commit();
    //     }
    // }

    // handle_image_t create_image(const gfx::config_image_t& config) {
    //     internal::image_t image{};
    //     image.handle_image = context.create_image(config);
    //     handle_image_t handle = utils::create_and_insert_new_handle<handle_image_t>(_images, image);
    //     horizon_trace("created image");
    //     return handle;
    // }

    // void allocate_image_descriptor_set(handle_image_t handle, gfx::handle_descriptor_set_layout_t handle_descriptor_set_layout) {
    //     internal::image_t& image = utils::assert_and_get_data<internal::image_t>(handle, _images);
    //         image.handle_descriptor_set = context.allocate_descriptor_set({ .handle_descriptor_set_layout = handle_descriptor_set_layout });
    // }

    // gfx::handle_image_t image(handle_image_t handle) {
    //     internal::image_t& image = utils::assert_and_get_data<internal::image_t>(handle, _images);
    //     return image.handle_image;
    // }

    // gfx::handle_descriptor_set_t image_descriptor_set(handle_image_t handle) {
    //     internal::image_t& image = utils::assert_and_get_data<internal::image_t>(handle, _images);
    //     return image.handle_descriptor_set;
    // }

    // void update_image_descriptor_set(handle_image_t handle, uint32_t binding, const gfx::image_descriptor_info_t& info, uint32_t count = 1) {
    //     internal::image_t& image = utils::assert_and_get_data<internal::image_t>(handle, _images);
    //     context.update_descriptor_set(image.handle_descriptor_set).push_image_write(binding, info, count).commit();
    // }
    
    friend struct update_descriptor_set_t<MAX_FRAMES_IN_FLIGHT>;

    const core::window_t&       window;
    gfx::context_t              context;
    gfx::handle_swapchain_t     swapchain;
    gfx::handle_command_pool_t  command_pool;
    gfx::handle_commandbuffer_t commandbuffers[MAX_FRAMES_IN_FLIGHT];
    gfx::handle_fence_t         in_flight_fences[MAX_FRAMES_IN_FLIGHT];
    gfx::handle_semaphore_t     image_available_semaphores[MAX_FRAMES_IN_FLIGHT];
    gfx::handle_semaphore_t     render_finished_semaphores[MAX_FRAMES_IN_FLIGHT];

    uint32_t current_frame = 0;
    uint32_t next_image;

    std::map<handle_buffer_t, internal::buffer_t> _buffers;
    std::map<handle_descriptor_set_t, internal::descriptor_set_t> _descriptor_sets;
    // std::map<handle_image_t, internal::image_t> _images;
};

template <size_t MAX_FRAMES_IN_FLIGHT>
struct update_descriptor_set_t {
    update_descriptor_set_t& push_buffer_write(uint32_t binding, const buffer_descriptor_info_t& info, uint32_t count) {
        horizon_profile();
        internal::descriptor_set_t& descriptor_set = utils::assert_and_get_data<internal::descriptor_set_t>(handle, renderer._descriptor_sets);

        VkDescriptorBufferInfo *vk_buffer_info = new VkDescriptorBufferInfo;
        vk_buffer_info->buffer = utils::assert_and_get_data<internal::buffer_t>(info.handle_buffer, context._buffers);
        vk_buffer_info->offset = info.vk_offset;
        vk_buffer_info->range = info.vk_range;
        
        internal::descriptor_set_layout_t& descriptor_set_layout = utils::assert_and_get_data<internal::descriptor_set_layout_t>(descriptor_set.config.handle_descriptor_set_layout, context._descriptor_set_layouts);
        auto itr = std::find_if(descriptor_set_layout.config.vk_descriptor_set_layout_bindings.begin(),
                                descriptor_set_layout.config.vk_descriptor_set_layout_bindings.end(),
                                [binding](const VkDescriptorSetLayoutBinding& layout_binding) {
                                    return binding == layout_binding.binding;
                                });
        assert(itr != descriptor_set_layout.config.vk_descriptor_set_layout_bindings.end());
        vk_write.descriptorType = itr->descriptorType;
        vk_write.pBufferInfo = vk_buffer_info;
        vk_writes.push_back(vk_write);

        if (descriptor_set.resource_usage == resource_usage_t::e_update_once) {
            VkWriteDescriptorSet vk_write{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            vk_write.dstBinding = binding;
            vk_write.descriptorCount = count;
            vk_write.dstSet = renderer.context.get_descriptor_set(descriptor_set.handle_descriptor_sets[0]);
            VkDescriptorBufferInfo *vk_buffer_info = new VkDescriptorBufferInfo;
            vk_buffer_info->buffer = renderer.context.get_buffer(info.handle_buffer);
            vk_buffer_info->offset = info.vk_offset;
            vk_buffer_info->range = info.vk_range;

            gfx::internal::descriptor_set_layout_t& descriptor_set_layout = renderer.context.get_descriptor_set_layout(renderer.context.get_descriptor_set(descriptor_set.handle_descriptor_sets[0]).config.handle_descriptor_set_layout);
            auto itr = std::find_if(descriptor_set_layout.config.vk_descriptor_set_layout_bindings.begin(),
                                    descriptor_set_layout.config.vk_descriptor_set_layout_bindings.end(),
                                    [binding](const VkDescriptorSetLayoutBinding& layout_binding) {
                                        return binding == layout_binding.binding;
                                    });
            assert(itr != descriptor_set_layout.config.vk_descriptor_set_layout_bindings.end());
            vk_write.descriptorType = itr->descriptorType;
            vk_write.pBufferInfo = vk_buffer_info;
            vk_writes.push_back(vk_write);
        } else {
            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                VkWriteDescriptorSet vk_write{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
                vk_write.dstBinding = binding;
                vk_write.descriptorCount = count;
                vk_write.dstSet = renderer.context.get_descriptor_set(descriptor_set.handle_descriptor_sets[i]);
                VkDescriptorBufferInfo *vk_buffer_info = new VkDescriptorBufferInfo;
                vk_buffer_info->buffer = renderer.context.get_sampler(info.handle_sampler);
                vk_buffer_info->offset = info.vk_offset;
                vk_buffer_info->range = info.vk_range;

                gfx::internal::descriptor_set_layout_t& descriptor_set_layout = renderer.context.get_descriptor_set_layout(renderer.context.get_descriptor_set(descriptor_set.handle_descriptor_sets[i]).config.handle_descriptor_set_layout);
                auto itr = std::find_if(descriptor_set_layout.config.vk_descriptor_set_layout_bindings.begin(),
                                        descriptor_set_layout.config.vk_descriptor_set_layout_bindings.end(),
                                        [binding](const VkDescriptorSetLayoutBinding& layout_binding) {
                                            return binding == layout_binding.binding;
                                        });
                assert(itr != descriptor_set_layout.config.vk_descriptor_set_layout_bindings.end());
                vk_write.descriptorType = itr->descriptorType;
                vk_write.pBufferInfo = vk_buffer_info;
                vk_writes.push_back(vk_write);
            }
        }

        return *this;
    }
    update_descriptor_set_t& push_image_write(uint32_t binding, const image_descriptor_info_t& info, uint32_t count) {
        horizon_profile();
        internal::descriptor_set_t& descriptor_set = utils::assert_and_get_data<internal::descriptor_set_t>(handle, renderer._descriptor_sets);
        if (descriptor_set.resource_usage == resource_usage_t::e_update_once) {
            VkWriteDescriptorSet vk_write{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            vk_write.dstBinding = binding;
            vk_write.descriptorCount = count;
            vk_write.dstSet = renderer.context.get_descriptor_set(descriptor_set.handle_descriptor_sets[0]);
            VkDescriptorImageInfo *vk_image_info = new VkDescriptorImageInfo;
            vk_image_info->sampler = renderer.context.get_sampler(info.handle_sampler);
            vk_image_info->imageView = renderer.context.get_image_view(info.handle_image_view);
            vk_image_info->imageLayout = info.vk_image_layout;

            gfx::internal::descriptor_set_layout_t& descriptor_set_layout = renderer.context.get_descriptor_set_layout(renderer.context.get_descriptor_set(descriptor_set.handle_descriptor_sets[0]).config.handle_descriptor_set_layout);
            auto itr = std::find_if(descriptor_set_layout.config.vk_descriptor_set_layout_bindings.begin(),
                                    descriptor_set_layout.config.vk_descriptor_set_layout_bindings.end(),
                                    [binding](const VkDescriptorSetLayoutBinding& layout_binding) {
                                        return binding == layout_binding.binding;
                                    });
            assert(itr != descriptor_set_layout.config.vk_descriptor_set_layout_bindings.end());
            vk_write.descriptorType = itr->descriptorType;
            vk_write.pImageInfo = vk_image_info;
            vk_writes.push_back(vk_write);
        } else {
            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                VkWriteDescriptorSet vk_write{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
                vk_write.dstBinding = binding;
                vk_write.descriptorCount = count;
                vk_write.dstSet = renderer.context.get_descriptor_set(descriptor_set.handle_descriptor_sets[i]);
                VkDescriptorImageInfo *vk_image_info = new VkDescriptorImageInfo;
                vk_image_info->sampler = renderer.context.get_sampler(info.handle_sampler);
                vk_image_info->imageView = renderer.context.get_image_view(info.handle_image_view);
                vk_image_info->imageLayout = info.vk_image_layout;

                gfx::internal::descriptor_set_layout_t& descriptor_set_layout = renderer.context.get_descriptor_set_layout(renderer.context.get_descriptor_set(descriptor_set.handle_descriptor_sets[i]).config.handle_descriptor_set_layout);
                auto itr = std::find_if(descriptor_set_layout.config.vk_descriptor_set_layout_bindings.begin(),
                                        descriptor_set_layout.config.vk_descriptor_set_layout_bindings.end(),
                                        [binding](const VkDescriptorSetLayoutBinding& layout_binding) {
                                            return binding == layout_binding.binding;
                                        });
                assert(itr != descriptor_set_layout.config.vk_descriptor_set_layout_bindings.end());
                vk_write.descriptorType = itr->descriptorType;
                vk_write.pImageInfo = vk_image_info;
                vk_writes.push_back(vk_write);
            }
        }
        return *this;
    }

    void commit() {
        horizon_profile();
        vkUpdateDescriptorSets(renderer.context._vkb_device, vk_writes.size(), vk_writes.data(), 0, nullptr);
        for (auto& vk_write : vk_writes) {
            if (vk_write.pBufferInfo) delete vk_write.pBufferInfo;
            if (vk_write.pImageInfo) delete vk_write.pImageInfo;
        }    
        vk_writes.clear();
        horizon_trace("updated descriptor set {}", handle);
    }

    renderer_t<MAX_FRAMES_IN_FLIGHT>& renderer;
    handle_descriptor_set_t handle = null_handle;
    std::vector<VkWriteDescriptorSet> vk_writes;
};

int main() {
    core::window_t window{ "test", 640, 420 };
    renderer_t<2> renderer{ window };

    // {
    //     gfx::context_t context{ true };
    //     gfx::config_descriptor_set_layout_t cdsl{};
    //     cdsl.add_layout_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    //     auto dsl = context.create_descriptor_set_layout(cdsl);
    //     gfx::config_image_t config_image{};
    //     config_image.vk_width = 640;
    //     config_image.vk_height = 420;
    //     config_image.vk_depth = 1;
    //     config_image.vk_type = VK_IMAGE_TYPE_2D;
    //     config_image.vk_format = VK_FORMAT_R8G8B8A8_SRGB;
    //     config_image.vk_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    //     config_image.vk_mips = 1;
    //     auto image = renderer.create_image(config_image);
    //     renderer.allocate_image_descriptor_set()
    // }

    gfx::config_descriptor_set_layout_t config_descriptor_set_layout{};
    config_descriptor_set_layout.add_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    gfx::handle_descriptor_set_layout_t descriptor_set_layout = renderer.context.create_descriptor_set_layout(config_descriptor_set_layout);

    gfx::config_pipeline_layout_t config_pipeline_layout{};
    config_pipeline_layout.add_descriptor_set_layout(descriptor_set_layout);
    auto pipeline_layout = renderer.context.create_pipeline_layout(config_pipeline_layout);
    gfx::config_pipeline_t config_pipeline{};
    gfx::config_shader_t config_shader{};
    config_shader.code = test_vertex;
    config_shader.name = "test_vertex";
    config_shader.type = gfx::shader_type_t::e_vertex;
    auto vertex_shader = renderer.context.create_shader(config_shader);
    config_shader.code = test_fragment;
    config_shader.name = "test_fragment";
    config_shader.type = gfx::shader_type_t::e_fragment;
    auto fragment_shader = renderer.context.create_shader(config_shader);
    config_pipeline.add_shader(vertex_shader);
    config_pipeline.add_shader(fragment_shader);
    config_pipeline.add_color_attachment(VK_FORMAT_B8G8R8A8_SRGB, gfx::default_color_blend_attachment());
    config_pipeline.handle_pipeline_layout = pipeline_layout;
    auto pipeline = renderer.context.create_graphics_pipeline(config_pipeline);

    auto command_pool = renderer.context.create_command_pool({});

    const uint32_t MAX_FRAMES_IN_FLIGHT = 3;
    uint32_t current_frame = 0;

    gfx::handle_commandbuffer_t commandbuffers[MAX_FRAMES_IN_FLIGHT];
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        commandbuffers[i] = renderer.context.allocate_commandbuffer({ .handle_command_pool = command_pool });
    }
    gfx::handle_fence_t in_flight_fences[MAX_FRAMES_IN_FLIGHT];
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        in_flight_fences[i] = renderer.context.create_fence({});
    }
    gfx::handle_semaphore_t image_available_semaphores[MAX_FRAMES_IN_FLIGHT];
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        image_available_semaphores[i] = renderer.context.create_semaphore({});
    }
    gfx::handle_semaphore_t render_finished_semaphores[MAX_FRAMES_IN_FLIGHT];
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        render_finished_semaphores[i] = renderer.context.create_semaphore({});
    }

    gfx::config_buffer_t config_buffer{};
    config_buffer.vk_size = sizeof(float);
    config_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    config_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    auto buffer = renderer.create_buffer(resource_usage_t::e_update_each_frame, config_buffer);
    renderer.allocate_buffer_descriptor_set(buffer, descriptor_set_layout);
    buffer_descriptor_info_t buffer_descriptor_info{};
    buffer_descriptor_info.handle_buffer = buffer;
    buffer_descriptor_info.vk_offset = 0;
    buffer_descriptor_info.vk_range = VK_WHOLE_SIZE;
    renderer.update_buffer_descriptor_set(buffer, 0, buffer_descriptor_info);

    VkViewport swapchain_viewport{};
    swapchain_viewport.x = 0;
    swapchain_viewport.y = 0;
    swapchain_viewport.width = 640;
    swapchain_viewport.height = 420;
    swapchain_viewport.minDepth = 0;
    swapchain_viewport.maxDepth = 1;
    VkRect2D swapchain_scissor{};
    swapchain_scissor.offset = {0, 0};
    swapchain_scissor.extent = {640, 420};

    float current_val = 0;

    while (!window.should_close()) {
        core::window_t::poll_events();

        current_val += .0001;
        if (current_val > 1.0) current_val = 0;

        *reinterpret_cast<float *>(renderer.context.map_buffer(renderer.buffer(buffer))) = current_val;

        renderer.begin();
 
        auto rendering_attachment = renderer.get_swapchain_rendering_attachment({0, 0, 0, 0}, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
        renderer.cmd_begin_rendering({rendering_attachment}, std::nullopt, VkRect2D{VkOffset2D{}, {640, 420}});
        renderer.cmd_bind_pipeliine(pipeline);
        renderer.cmd_bind_descriptor_sets(pipeline, 0, { renderer.buffer_descriptor_set(buffer) });
        renderer.cmd_set_viewport_and_scissor(swapchain_viewport, swapchain_scissor);
        renderer.cmd_draw(6, 1, 0, 0);
        renderer.cmd_end_rendering();
        
        renderer.end();
    }

    return 0;
}
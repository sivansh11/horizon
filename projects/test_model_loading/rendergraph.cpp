#include "rendergraph.hpp"

#include <set>

namespace rendergraph {

void transition_buffer(resource_t& resource, resource_usage_t target_usage, renderer::base_renderer_t& renderer) {
    auto commandbuffer = renderer.current_commandbuffer();

    switch (resource.usage) {
        case resource_usage_t::e_undefined: {
            switch (target_usage) {
                case resource_usage_t::e_reading:
                    // prefilled buffer assumed 
                    break;
                case resource_usage_t::e_writing:
                    // buffer content dont care
                    break;
                case resource_usage_t::e_read_write:
                    // really dont know what to do here ?
                    break;
                default:
                    check(false, "reached unreachable");
                    
            }
        }
        break;
        case resource_usage_t::e_reading: {
            switch (target_usage) {
                case resource_usage_t::e_reading:
                    // already reading
                    break;
                case resource_usage_t::e_writing:
                    // transition to writing
                    break;
                case resource_usage_t::e_read_write:
                    // transition to read write 
                    break;
                default:
                    check(false, "reached unreachable");
            }
        }
        break;
        case resource_usage_t::e_writing: {
            switch (target_usage) {
                case resource_usage_t::e_reading:
                    // transition to reading
                    break;
                case resource_usage_t::e_writing:
                    // already writing
                    break;
                case resource_usage_t::e_read_write:
                    // transition to read write 
                    break;
                default:
                    check(false, "reached unreachable");
            }
        }
        break;
        case resource_usage_t::e_read_write: {
            switch (target_usage) {
                case resource_usage_t::e_reading:
                    // transition to reading
                    break;
                case resource_usage_t::e_writing:
                    // transition to writing
                    break;
                case resource_usage_t::e_read_write:
                    // already read write 
                    break;
                default:
                    check(false, "reached unreachable");
            }
        }
        break;
    }
}

void transition_image_color(resource_t& resource, resource_usage_t target_usage, renderer::base_renderer_t& renderer) {
    auto commandbuffer = renderer.current_commandbuffer();

    switch (resource.usage) {
        case resource_usage_t::e_undefined: {
            switch (target_usage) {
                case resource_usage_t::e_reading:
                    renderer.context.cmd_image_memory_barrier(commandbuffer, resource.as.image.handle, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
                    horizon_warn("images should already have reading usage if its prefilled");
                    break;
                case resource_usage_t::e_writing:
                    if (resource.type == resource_type_t::e_image_color)
                        renderer.context.cmd_image_memory_barrier(commandbuffer, resource.as.image.handle, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
                    else if (resource.type == resource_type_t::e_image_depth)
                        renderer.context.cmd_image_memory_barrier(commandbuffer, resource.as.image.handle, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
                    break;
                case resource_usage_t::e_read_write:
                    renderer.context.cmd_image_memory_barrier(commandbuffer, resource.as.image.handle, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
                    break;
                default:
                    check(false, "reached unreachable");
                    
            }
        }
        break;
        case resource_usage_t::e_reading: {
            switch (target_usage) {
                case resource_usage_t::e_reading:
                    // already reading
                    break;
                case resource_usage_t::e_writing:
                    if (resource.type == resource_type_t::e_image_color)
                        renderer.context.cmd_image_memory_barrier(commandbuffer, resource.as.image.handle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
                    else if (resource.type == resource_type_t::e_image_depth)
                        renderer.context.cmd_image_memory_barrier(commandbuffer, resource.as.image.handle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
                    break;
                case resource_usage_t::e_read_write:
                    renderer.context.cmd_image_memory_barrier(commandbuffer, resource.as.image.handle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
                    break;
                default:
                    check(false, "reached unreachable");
            }
        }
        break;
        case resource_usage_t::e_writing: {
            switch (target_usage) {
                case resource_usage_t::e_reading:
                    if (resource.type == resource_type_t::e_image_color)
                        renderer.context.cmd_image_memory_barrier(commandbuffer, resource.as.image.handle, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
                    else if (resource.type == resource_type_t::e_image_depth)
                        renderer.context.cmd_image_memory_barrier(commandbuffer, resource.as.image.handle, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
                    // renderer.context.cmd_image_memory_barrier(commandbuffer, resource.as.image.handle, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
                    break;
                case resource_usage_t::e_writing:
                    // already writing
                    break;
                case resource_usage_t::e_read_write:
                    if (resource.type == resource_type_t::e_image_color)
                        renderer.context.cmd_image_memory_barrier(commandbuffer, resource.as.image.handle, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
                    else if (resource.type == resource_type_t::e_image_depth)
                        renderer.context.cmd_image_memory_barrier(commandbuffer, resource.as.image.handle, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
                    // renderer.context.cmd_image_memory_barrier(commandbuffer, resource.as.image.handle, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
                    break;
                default:
                    check(false, "reached unreachable");
            }
        }
        break;
        case resource_usage_t::e_read_write: {
            switch (target_usage) {
                case resource_usage_t::e_reading:
                    // transition to reading
                    check(false, "reached unreachable");
                    break;
                case resource_usage_t::e_writing:
                    // transition to writing
                    check(false, "reached unreachable");
                    break;
                case resource_usage_t::e_read_write:
                    // already read write 
                    break;
                default:
                    check(false, "reached unreachable");
            }
        }
        break;
    }
}

resource_builder_t::resource_builder_t(std::string name, resource_usage_t usage)
  : resource(name, usage) {}

resource_t resource_builder_t::is_buffer(renderer::handle_buffer_t handle_buffer) {
    resource.type = resource_type_t::e_buffer;
    resource.as.buffer.handle = handle_buffer;
    return resource;
}

// resource_t resource_builder_t::is_image(gfx::handle_image_t handle_image, gfx::handle_image_view_t handle_image_view, gfx::handle_sampler_t handle_sampler) {
resource_t resource_builder_t::is_image(gfx::handle_image_t handle_image, gfx::handle_image_view_t handle_image_view) {
    resource.type = resource_type_t::e_image_color;
    resource.as.image.handle = handle_image;
    resource.as.image.handle_view = handle_image_view;
    // resource.as.image.handle_sampler = handle_sampler;
    return resource;
}

task_builder_t::task_builder_t(std::string name, task_type_t type)
  : task(name, type) {}

task_builder_t& task_builder_t::add_resource_refrenced_and_its_usage(resource_t& resource, resource_usage_t usage) {
    task.resources.push_back({&resource, usage});
    return *this;
}

task_t task_builder_t::with_callback(task_callback_t callback) {
    task.callback = callback;
    return task;
}

rendergraph_t& rendergraph_t::add_task(const task_t& task) {
    tasks.push_back(task);
    return *this;
}

void rendergraph_t::run(renderer::base_renderer_t& renderer) {
    for (auto& task : tasks) {
        for (auto& [resource, usage] : task.resources) {
            if (resource->usage != usage) {
                if (resource->type == resource_type_t::e_buffer) transition_buffer(*resource, usage, renderer);
                if (resource->type == resource_type_t::e_image_color) transition_image_color(*resource, usage, renderer);
                resource->usage = usage;
            }
        }
        task.callback(renderer);
    }
}

} // namespace rendergraph
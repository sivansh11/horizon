#ifndef RENDERGRAPH_HPP
#define RENDERGRAPH_HPP

#include "gfx/base_renderer.hpp"

#include <map>
#include <vector>
#include <string>

namespace rendergraph {

define_handle(handle_resource_t);

enum class resource_usage_t {
    e_undefined,
    e_reading,
    e_writing,
    e_read_write,  // general
};


enum class resource_type_t {
    e_buffer,
    e_image_color,
    e_image_depth,
};

struct resource_t {
    std::string                       name;
    resource_usage_t                  usage;
    resource_type_t                   type;
    union {
        struct {
            renderer::handle_buffer_t handle;
        }                             buffer;
        struct {
            gfx::handle_image_t       handle;
            gfx::handle_image_view_t  handle_view;
            gfx::handle_sampler_t     handle_sampler;
        }                             image ;
    }                                 as;

    bool operator==(const resource_t& other) {
        bool boolean = name  == other.name && 
                       usage == other.usage &&
                       type  == other.type;
        switch (type) {
            case resource_type_t::e_buffer:
                boolean = boolean && as.buffer.handle == other.as.buffer.handle;
                break;
            case resource_type_t::e_image_color:
                boolean = boolean && as.image.handle         == other.as.image.handle &&
                                     as.image.handle_view    == other.as.image.handle_view &&
                                     as.image.handle_sampler == other.as.image.handle_sampler;
        }
        return boolean;
    }
};

} // namespace rendergraph

namespace std {

template <>
struct hash<rendergraph::resource_t> {
    size_t operator()(rendergraph::resource_t const &resource) const {
        size_t seed = 0;  // seed is u64 while all my params are u32, might be an issue ??
        core::hash_combine(seed, 
                           std::hash<std::string>{}(resource.name),
                           std::hash<rendergraph::resource_usage_t>{}(resource.usage),
                           std::hash<rendergraph::resource_type_t>{}(resource.type));
        // if (resource.type == rendergraph::resource_type_t::e_buffer) {}
        switch (resource.type) {
            case rendergraph::resource_type_t::e_buffer:
                core::hash_combine(seed, std::hash<handle_t>{}(resource.as.buffer.handle.val));
                break;
            case rendergraph::resource_type_t::e_image_color:
                core::hash_combine(seed, 
                                   std::hash<handle_t>{}(resource.as.image.handle.val),
                                   std::hash<handle_t>{}(resource.as.image.handle_view.val),
                                   std::hash<handle_t>{}(resource.as.image.handle_sampler.val));
                break;
        }
        return seed;
    }
};

}  // namespace std

namespace rendergraph {

struct resource_builder_t {
    resource_builder_t(std::string name, resource_usage_t usage = resource_usage_t::e_undefined);
    resource_t is_buffer(renderer::handle_buffer_t handle_buffer);
    resource_t is_image(gfx::handle_image_t handle_image, gfx::handle_image_view_t handle_image_view, gfx::handle_sampler_t handle_sampler = null_handle);

    resource_t resource;  
};

enum class task_type_t {
    e_graphics,
    e_cpu,
    e_compute,
};

using task_callback_t = std::function<void(renderer::renderer_t&)>;

struct task_t {
    std::string name;
    task_type_t type;
    std::vector<std::pair<rendergraph::resource_t *, resource_usage_t>> resources;
    task_callback_t callback;
};

struct task_builder_t {
    task_builder_t(std::string name, task_type_t type);
    task_builder_t& add_resource_refrenced_and_its_usage(rendergraph::resource_t& resource, resource_usage_t usage);
    task_t with_callback(task_callback_t callback);
    task_t task;
};

void transition_buffer(resource_t& resource, resource_usage_t target_usage, renderer::base_renderer_t& renderer);
void transition_image_color(resource_t& resource, resource_usage_t target_usage, renderer::base_renderer_t& renderer);

struct rendergraph_t {
    rendergraph_t& add_task(const task_t& task);
    void run(renderer::base_renderer_t& renderer);
    std::vector<task_t> tasks;
};

} // namespace rendergraph

#endif
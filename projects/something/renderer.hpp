#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "gfx/context.hpp"
#include "gfx/base_renderer.hpp"

#include "utility.hpp"
#include "editor_camera.hpp"

#include <glm/glm.hpp>

struct gpu_mesh_t {
    gfx::handle_buffer_t vertex_buffer;
    gfx::handle_buffer_t index_buffer;
    uint32_t vertex_count;
    uint32_t index_count;
    // material ?
};

struct camera_uniform_t {
    glm::mat4 view;
    glm::mat4 inv_view;
    glm::mat4 projection;
    glm::mat4 inv_projection;
};

struct additional_uniform_t {
    glm::vec3 view_position;
    glm::vec3 direction;
};

struct renderer_t {
    renderer_t(core::window_t& window);
    void render(editor_camera_t& camera, const std::vector<gpu_mesh_t>& gpu_meshes);

    core::window_t& window;
    renderer::base_renderer_t base_renderer;

    gfx::handle_descriptor_set_layout_t single_combined_image_sampler_descriptor_set_layout;

    gfx::handle_pipeline_layout_t swapchain_pipeline_layout;
    
    gfx::handle_pipeline_t swapchain_pipeline;
    gfx::handle_pipeline_t pipeline;
    
    renderer::handle_descriptor_set_t swapchain_descriptor_set;
    renderer::handle_descriptor_set_t camera_descriptor_set;
    renderer::handle_descriptor_set_t additional_descriptor_set;

    renderer::handle_buffer_t camera_buffer;
    renderer::handle_buffer_t additional_buffer;

    gfx::handle_image_t target_image;

    gfx::handle_image_view_t target_image_view;

    gfx::handle_sampler_t sampler;
};
#endif
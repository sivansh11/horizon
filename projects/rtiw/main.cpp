#include "core/core.hpp"
#include "core/window.hpp"
#include "core/bvh.hpp"

#include "gfx/context.hpp"
#include "gfx/helper.hpp"
#include "gfx/base_renderer.hpp"

#include "utility.hpp"

#include <imgui.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

static const core::bvh_t::build_options_t build_options{
    .primitive_intersection_cost = 1.1,
    .node_intersection_cost = 1,
    .min_primitive_count = 1,
    .max_primitive_count = std::numeric_limits<uint32_t>::max(),
    .sah_samples = 25,
    .add_node_intersection_cost_in_leaf_traversal = false,
};

struct sphere_t {
    core::aabb_t aabb() const {
        return { _center - glm::vec3{ r }, _center + glm::vec3{ r }};
    }

    glm::vec3 center() const {
        return _center;
    }

    glm::vec3 _center;
    float r;
    uint32_t material_id;
};

static uint32_t material_type_lambertian = 0;
static uint32_t material_type_metal = 1;
static uint32_t material_type_dielectric = 2;
static uint32_t material_type_diffuse_light = 3;

struct material_t {
    uint32_t material_type = material_type_lambertian;
    glm::vec3 albedo;
    float ri_or_fuzz;
};

struct gpu_buffer_t {
    operator VkDeviceAddress() const {
        return p_buffer;
    }
    gfx::handle_buffer_t handle;
    VkDeviceAddress p_buffer;
};

struct gpu_node_t {
    core::vec3   min;
    uint32_t        primitive_count;
    core::vec3   max;
    uint32_t        first_index;
};

struct gpu_bvh_t {
    VkDeviceAddress p_nodes;
    VkDeviceAddress p_primitive_indices;
    uint32_t node_count;
    uint32_t primitive_count;
    VkDeviceAddress p_parent_ids;
};

gpu_buffer_t to_gpu(renderer::base_renderer_t& renderer, const void *p_data, size_t n) {
    gfx::config_buffer_t config_buffer{};
    config_buffer.vk_size = n;
    config_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    // config_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    gfx::handle_buffer_t buffer = renderer.context.create_buffer(config_buffer);

    gfx::config_buffer_t config_staging_buffer{};
    config_staging_buffer.vk_size = n;
    config_staging_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    config_staging_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    gfx::handle_buffer_t staging_buffer = renderer.context.create_buffer(config_staging_buffer);

    std::memcpy(renderer.context.map_buffer(staging_buffer), p_data, n);

    gfx::handle_commandbuffer_t commandbuffer = gfx::helper::start_single_use_commandbuffer(renderer.context, renderer.command_pool);
    renderer.context.cmd_copy_buffer(commandbuffer, staging_buffer, buffer, { .vk_size = config_buffer.vk_size });
    gfx::helper::end_single_use_commandbuffer(renderer.context, commandbuffer);

    renderer.context.destroy_buffer(staging_buffer);

    return { buffer, renderer.context.get_buffer_device_address(buffer) };
}

gpu_buffer_t to_gpu(renderer::base_renderer_t& renderer, const core::bvh_t& bvh) {
    gpu_bvh_t gpu_bvh;
    gpu_bvh.node_count = bvh.node_count;
    gpu_bvh.primitive_count = bvh.primitive_count;
    gpu_bvh.p_nodes = to_gpu(renderer, bvh.p_nodes, sizeof(core::node_t) * bvh.node_count);
    gpu_bvh.p_primitive_indices = to_gpu(renderer, bvh.p_primitive_indices, sizeof(uint32_t) * bvh.primitive_count);
    gpu_bvh.p_parent_ids = to_gpu(renderer, bvh.p_parent_ids, sizeof(uint32_t) * bvh.node_count);
    return to_gpu(renderer, &gpu_bvh, sizeof(gpu_bvh));
}

gpu_buffer_t to_gpu(renderer::base_renderer_t& renderer, const std::vector<sphere_t>& spheres) {
    return to_gpu(renderer, spheres.data(), spheres.size() * sizeof(sphere_t));
}

gpu_buffer_t to_gpu(renderer::base_renderer_t& renderer, const std::vector<material_t>& materials) {
    return to_gpu(renderer, materials.data(), materials.size() * sizeof(material_t));
}

core::bvh_t build_bvh(const std::vector<sphere_t>& spheres) {
    std::vector<core::aabb_t> aabbs{};
    std::vector<glm::vec3> centers{};
    for (auto& sphere : spheres) {
        aabbs.push_back(sphere.aabb());
        centers.push_back(sphere.center());
    }

    return core::bvh_t::construct(aabbs.data(), centers.data(), spheres.size(), build_options);
}

int main(int argc, char **argv) {

    core::window_t window{ "rtiw", 800, 800 };
    gfx::context_t context{ true };

    auto [width, height] = window.dimensions();

    gfx::config_image_t config_target_image{};
    config_target_image.vk_width = width;
    config_target_image.vk_height = height;
    config_target_image.vk_depth = 1;
    config_target_image.vk_type = VK_IMAGE_TYPE_2D;
    config_target_image.vk_format = VK_FORMAT_R32G32B32A32_SFLOAT;
    config_target_image.vk_usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    config_target_image.vk_mips = 1;
    gfx::handle_image_t target_image = context.create_image(config_target_image);
    gfx::handle_image_view_t target_image_view = context.create_image_view({ .handle_image = target_image });

    gfx::handle_sampler_t sampler = context.create_sampler({});

    renderer::base_renderer_t renderer{ window, context, sampler, target_image_view };

    gfx::helper::imgui_init(window, renderer, VK_FORMAT_R32G32B32_SFLOAT);

    gfx::config_descriptor_set_layout_t config_rtiw_descriptor_set_layout{};
    // TODO: auto incrementing maybe ?
    config_rtiw_descriptor_set_layout.add_layout_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                                     .add_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    gfx::handle_descriptor_set_layout_t rtiw_descriptor_set_layout = context.create_descriptor_set_layout(config_rtiw_descriptor_set_layout);

    gfx::config_pipeline_layout_t config_rtiw_pipeline_layout{};
    config_rtiw_pipeline_layout.add_descriptor_set_layout(rtiw_descriptor_set_layout);
    gfx::handle_pipeline_layout_t rtiw_pipeline_layout = context.create_pipeline_layout(config_rtiw_pipeline_layout);

    gfx::config_pipeline_t config_rtiw_pipeline{};
    config_rtiw_pipeline.handle_pipeline_layout = rtiw_pipeline_layout;
    config_rtiw_pipeline.add_shader(context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/rtiw/comp.slang", .name = "raytracing shader", .type = gfx::shader_type_t::e_compute }));
    gfx::handle_pipeline_t rtiw_pipeline = context.create_compute_pipeline(config_rtiw_pipeline);

    std::vector<material_t> materials{
        { material_type_lambertian, glm::vec3{ 0.1, 0.2, 0.5 } },  // center
        { material_type_lambertian, glm::vec3{ 0.8, 0.8, 0.0 } },  // ground
        { material_type_dielectric, glm::vec3{ 1, 1, 1 }, 1.5 },  // left
        { material_type_metal, glm::vec3{ 0.8, 0.6, 0.2 }, 0.1 },  // right
        { material_type_diffuse_light, glm::vec3{ 1, 1, 1 } },  // light
    };

    std::vector<sphere_t> spheres{ 
        { { 0, 0, -1      }, 0.5, 0 },
        { { 0, -100.5, -1 }, 100, 1 },
        { {-1, 0, -1      }, 0.5, 2 },
        { {-2, 0, -1      }, -0.5, 2 },
        { {-2, 0, -3      }, -0.5, 0 },
        { {-1, 0, -3      }, -0.5, 0 },
        { { 1, 0, -1      }, 0.5, 3 },
        { { -1, 100, 0    }, 50, 4 },
    };

    auto bvh = build_bvh(spheres);

    auto bvh_gpu_buffer = to_gpu(renderer, bvh);
    auto spheres_gpu_buffer = to_gpu(renderer, spheres);
    auto materials_gpu_buffer = to_gpu(renderer, materials);

    struct uniform_t {
        glm::mat4 inverse_projection;
        glm::mat4 inverse_view;
        uint32_t samples;
        uint32_t width, height;
        uint32_t sphere_count;
        VkDeviceAddress p_bvh;
        VkDeviceAddress p_spheres;
        VkDeviceAddress p_materials;
    };

    gfx::config_buffer_t config_uniform_buffer{};
    config_uniform_buffer.vk_size = sizeof(uniform_t);
    config_uniform_buffer.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    config_uniform_buffer.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    renderer::handle_buffer_t uniform_buffer = renderer.create_buffer(renderer::resource_policy_t::e_every_frame, config_uniform_buffer);

    renderer::handle_descriptor_set_t rtiw_descriptor_set = renderer.allocate_descriptor_set(renderer::resource_policy_t::e_every_frame, { .handle_descriptor_set_layout = rtiw_descriptor_set_layout });
    renderer.update_descriptor_set(rtiw_descriptor_set) 
        .push_image_write(1, gfx::image_descriptor_info_t{ .handle_sampler = sampler, .handle_image_view = target_image_view, .vk_image_layout = VK_IMAGE_LAYOUT_GENERAL })
        .push_buffer_write(0, { .handle_buffer = uniform_buffer })
        .commit();

    editor_camera_t camera{ window };

    float target_fps = 10000000.f;
    auto last_time = std::chrono::system_clock::now();

    core::frame_timer_t frame_timer{ 60.f };

    gfx::handle_timer_t timer = context.create_timer({});

    glm::mat4 old_inverse_projection = camera.inverse_projection();
    glm::mat4 old_inverse_view = camera.inverse_view();

    while (!window.should_close()) {
        core::clear_frame_function_times();
        core::window_t::poll_events();
        if (glfwGetKey(window.window(), GLFW_KEY_ESCAPE)) break;
        if (glfwGetKey(window.window(), GLFW_KEY_RIGHT_SHIFT)) {
            camera.camera_speed_multiplyer = 100;
        } else {
            camera.camera_speed_multiplyer = 1;   
        }
        if (glfwGetKey(window.window(), GLFW_KEY_R)) {
            context.wait_idle();
            context.destroy_pipeline(rtiw_pipeline);
            gfx::config_pipeline_t config_rtiw_pipeline{};
            config_rtiw_pipeline.handle_pipeline_layout = rtiw_pipeline_layout;
            config_rtiw_pipeline.add_shader(context.create_shader(gfx::config_shader_t{ .code_or_path = "../../assets/shaders/rtiw/comp.slang", .name = "raytracing shader", .type = gfx::shader_type_t::e_compute }));
            rtiw_pipeline = context.create_compute_pipeline(config_rtiw_pipeline);

        }

        auto current_time = std::chrono::system_clock::now();
        auto time_difference = current_time - last_time;
        if (time_difference.count() / 1e6 < 1000.f / target_fps) {
            continue;
        }
        last_time = current_time;

        auto dt = frame_timer.update();
        camera.update(dt.count());

        renderer.begin();
        auto commandbuffer = renderer.current_commandbuffer();
        auto [viewport, scissor] = gfx::helper::fill_viewport_and_scissor_structs(width, height);

        static bool first_frame = true;
        if (first_frame) {
            first_frame = false;
            // clear the framebuffer
            // context.cmd_image_memory_barrier(commandbuffer, target_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, 0, 0, 0);
            // gfx::rendering_attachment_t color_rendering_attachment{};
            // color_rendering_attachment.clear_value = {0, 0, 0, 0};
            // color_rendering_attachment.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            // color_rendering_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
            // color_rendering_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
            // color_rendering_attachment.handle_image_view = target_image_view;
            // context.cmd_begin_rendering(commandbuffer, { color_rendering_attachment }, std::nullopt, VkRect2D{VkOffset2D{}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}});

            // context.cmd_end_rendering(commandbuffer);

            // context.cmd_image_memory_barrier(commandbuffer, target_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0, 0, 0);
        }

        uniform_t *uniform = reinterpret_cast<uniform_t *>(context.map_buffer(renderer.buffer(uniform_buffer)));

        static int i = 0;
        i++;
        if (i == 1 || i == 2) {
            uniform->samples = 1;
        }
        glm::mat4 new_inverse_projection = camera.inverse_projection();
        glm::mat4 new_inverse_view = camera.inverse_view();
        if (old_inverse_projection == new_inverse_projection && old_inverse_view == new_inverse_view) {
            uniform->samples += 1;
        } else {
            uniform->samples = 1;
            old_inverse_projection = new_inverse_projection;
            old_inverse_view = new_inverse_view;
        }
        uniform->inverse_projection = new_inverse_projection;
        uniform->inverse_view = new_inverse_view;
        uniform->width = width;
        uniform->height = height;
        uniform->sphere_count = spheres.size();
        uniform->p_bvh = bvh_gpu_buffer;
        uniform->p_spheres = spheres_gpu_buffer;
        uniform->p_materials = materials_gpu_buffer;

        // horizon_info("{} {}", glm::to_string(camera.position()), glm::to_string(camera.front()));

        context.cmd_image_memory_barrier(commandbuffer, target_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        
        context.cmd_begin_timer(commandbuffer, timer);
        context.cmd_bind_pipeline(commandbuffer, rtiw_pipeline);
        context.cmd_bind_descriptor_sets(commandbuffer, rtiw_pipeline, 0, { renderer.descriptor_set(rtiw_descriptor_set) });
        context.cmd_dispatch(commandbuffer, 800 / 8, 800 / 8, 1);
        context.cmd_end_timer(commandbuffer, timer);

        static uint frame_count = 0;
        frame_count++;
        if (frame_count > 0) {
            frame_count = 0;
            if (auto t = context.timer_get_time(timer)) {
                horizon_info("gpu frame took: {}ms cpu frame took: {} samples accumulated: {}", *t, dt.count(), uniform->samples);
            } else {
                horizon_info("");
            }
        }

        // context.cmd_end_rendering(commandbuffer);
        context.cmd_image_memory_barrier(commandbuffer, target_image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, 0, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        // context.cmd_image_memory_barrier(commandbuffer, target_image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, 0, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        
        // context.cmd_image_memory_barrier(commandbuffer, target_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        // context.cmd_image_memory_barrier(commandbuffer, target_image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        // gfx::rendering_attachment_t color_rendering_attachment{};
        // color_rendering_attachment.clear_value = {0, 0, 0, 0};
        // color_rendering_attachment.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        // color_rendering_attachment.load_op = VK_ATTACHMENT_LOAD_OP_LOAD;
        // color_rendering_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
        // color_rendering_attachment.handle_image_view = target_image_view;
        // context.cmd_begin_rendering(commandbuffer, { color_rendering_attachment }, std::nullopt, VkRect2D{VkOffset2D{}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}});

        // gfx::helper::imgui_newframe();

        
        // gfx::helper::imgui_endframe(renderer, commandbuffer);

        // context.cmd_end_rendering(commandbuffer);

        // context.cmd_image_memory_barrier(commandbuffer, target_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

        renderer.end();
    }

    context.wait_idle();

    gfx::helper::imgui_shutdown();

    return 0;
}

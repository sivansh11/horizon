#include "core/core.hpp"
#include "core/window.hpp"
#include "core/bvh.hpp"
#include "core/random.hpp"

#include "gfx/context.hpp"
#include "gfx/helper.hpp"
#include "gfx/base_renderer.hpp"

#include "utility.hpp"
#include "renderer.hpp"

#include <imgui.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

static core::bvh_t::build_options_t build_options{
    .primitive_intersection_cost = 1.1f,
    .node_intersection_cost = 1.f,
    .min_primitive_count = 1,
    .max_primitive_count = std::numeric_limits<uint32_t>::max(),
    .sah_samples = 25,
    .add_node_intersection_cost_in_leaf_traversal = false,
};

struct gpu_buffer_t {
    operator VkDeviceAddress() const {
        return p_buffer;
    }
    gfx::handle_buffer_t handle;
    VkDeviceAddress p_buffer;
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

struct gpu_node_t {
    core::vec3   min;
    uint32_t     primitive_count;
    core::vec3   max;
    uint32_t     first_index;
};

struct gpu_bvh_t {
    VkDeviceAddress p_nodes;
    VkDeviceAddress p_primitive_indices;
    uint32_t node_count;
    uint32_t primitive_count;
};

gpu_buffer_t to_gpu(renderer::base_renderer_t& renderer, const core::bvh_t& bvh) {
    gpu_bvh_t gpu_bvh;
    gpu_bvh.node_count = bvh.node_count;
    gpu_bvh.primitive_count = bvh.primitive_count;
    gpu_bvh.p_nodes = to_gpu(renderer, bvh.p_nodes, sizeof(core::node_t) * bvh.node_count);
    gpu_bvh.p_primitive_indices = to_gpu(renderer, bvh.p_primitive_indices, sizeof(uint32_t) * bvh.primitive_count);
    return to_gpu(renderer, &gpu_bvh, sizeof(gpu_bvh));
}

void show_bvh_info(const core::bvh_t& bvh) {
    uint32_t max = 0;
    uint32_t min = std::numeric_limits<uint32_t>::max();
    float average = 0;
    uint32_t leaf_node_count = 0;
    for (uint32_t i = 0; i < bvh.node_count; i++) {
        auto& node = bvh.p_nodes[i];
        if (node.is_leaf()) {
            if (node.primitive_count > max) {
                max = node.primitive_count;
            }
            if (node.primitive_count < min) {
                min = node.primitive_count;
            }
            average += node.primitive_count;
            leaf_node_count++;
        }
    }

    horizon_info("\e[0;97mnode count: {}", bvh.node_count);
    horizon_info("\e[0;97mdepth: {}", bvh.depth());
    horizon_info("\e[0;97mglobal sah cost: {}", bvh.node_traversal_cost(build_options));
    horizon_info("\e[0;97mleaf count: {}", leaf_node_count);
    horizon_info("\e[0;97mmax tris per leaf: {}", max);
    horizon_info("\e[0;97mmin tris per leaf: {}", min);
    horizon_info("\e[0;97maverage tris per leaf: {}", average / float(leaf_node_count));
}

int main(int argc, char **argv) {
    core::window_t window{ "rtiw2", 800, 800 };
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

    clear_pass_t clear_pass{ window, renderer };
    clear_pass.update_target_image(sampler, target_image, target_image_view);
    clear_pass.update_uniform(width, height, { 0, 0, 0, 0 });

    std::shared_ptr<raytracer_pass_t> raytracer_pass = std::make_shared<raytracer_pass_t>(window, renderer);
    raytracer_pass->update_target_image(sampler, target_image, target_image_view);

    struct sphere_t {
        glm::vec3 center;
        float radius;
        uint32_t material_id;
    };

    struct triangle_t {
        core::vertex_t v0, v1, v2;
        uint32_t material_id;
    };

    static const uint32_t material_type_lambertian = 0;
    static const uint32_t material_type_diffuse_light = 1;

    struct material_t {
        uint32_t material_type;
        glm::vec3 albedo;
    };
    
    core::rng_t rng{ 0 };

    std::vector<material_t> materials{
        // material_t{ material_type_lambertian, { 1, 1, 1 } },
        // material_t{ material_type_diffuse_light, { 1, 1, 1 } },
    };

    for (int i = 0; i < 100; i++) {
        material_t material{ material_type_lambertian };
        material.albedo = rng.random3f();
        materials.push_back(material);
    }
    

    std::vector<triangle_t> triangles;
    std::vector<core::aabb_t> aabbs;
    std::vector<core::vec3> centers;

    std::string path = "../../assets/models/glTF-Sample-Models/2.0/Sponza/glTF/Sponza.gltf";
    // std::string path = "../../assets/models/corenl_box.obj";
    // std::string path = "../../assets/models/teapot.obj";
    // std::string path = "../../assets/models/dragon.obj";
    auto model = core::load_model_from_path(path);
    for (auto mesh : model.meshes) {
        for (uint32_t i = 0; i < mesh.indices.size(); i+=3) {
            // horizon_info("{}", mesh.name);
            triangle_t triangle {
                mesh.vertices[mesh.indices[i + 0]],
                mesh.vertices[mesh.indices[i + 1]],
                mesh.vertices[mesh.indices[i + 2]],
                0,
            };

            // if (mesh.name == "area_light") {
            //     triangle.material_id = 1;
            // }
            triangle.material_id = int(rng.randomf() * 23456788.f) % 10;

            core::aabb_t aabb{};
            aabb.grow(triangle.v0.position).grow(triangle.v1.position).grow(triangle.v2.position);
            core::vec3 center{};
            center = (triangle.v0.position + triangle.v1.position + triangle.v2.position) / 3.f;

            triangles.push_back(triangle);
            aabbs.push_back(aabb);
            centers.push_back(center);
        }
    }

    core::bvh_t bvh;
    if (std::filesystem::exists(path + ".bvh")) {
        horizon_warn("found cached bvh, loading that instead");
        bvh = core::bvh_t::load(path + ".bvh");
    } else {
        horizon_info("BUILDING BVH FROM SCRATCH");
        bvh = core::bvh_t::construct(aabbs.data(), centers.data(), triangles.size(), build_options);
        bvh.to_disk(path + ".bvh");
    }    
    show_bvh_info(bvh);
    // std::vector<sphere_t> spheres {
    //     sphere_t{ { 0,    0  , -1 },   0.5, 0 },
    //     sphere_t{ { 0, -100.5, -1 }, 100  , 1 },
    // };

    // gpu_buffer_t gpu_buffer_spheres = to_gpu(renderer, spheres.data(), spheres.size() * sizeof(sphere_t));

    gpu_buffer_t gpu_buffer_bvh = to_gpu(renderer, bvh);
    gpu_buffer_t gpu_buffer_triangles = to_gpu(renderer, triangles.data(), triangles.size() * sizeof(triangle_t));
    gpu_buffer_t gpu_buffer_materials = to_gpu(renderer, materials.data(), materials.size() * sizeof(material_t));

    editor_camera_t camera{ window };
    glm::mat4 old_inverse_projection = camera.inverse_projection();
    glm::mat4 old_inverse_view = camera.inverse_view();

    float target_fps = 10000000.f;
    auto last_time = std::chrono::system_clock::now();

    core::frame_timer_t frame_timer{ 60.f };

    uint32_t samples = 1;

    while (!window.should_close()) {
        core::clear_frame_function_times();
        core::window_t::poll_events();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE)) break;
        if (glfwGetKey(window, GLFW_KEY_R)) {
            context.wait_idle();
            raytracer_pass = std::make_shared<raytracer_pass_t>(window, renderer);
            raytracer_pass->update_target_image(sampler, target_image, target_image_view);
        }

        auto current_time = std::chrono::system_clock::now();
        auto time_difference = current_time - last_time;
        if (time_difference.count() / 1e6 < 1000.f / target_fps) {
            continue;
        }
        last_time = current_time;

        auto dt = frame_timer.update();
        camera.update(dt.count());

        bool should_clear = false;

        glm::mat4 new_inverse_projection = camera.inverse_projection();
        glm::mat4 new_inverse_view = camera.inverse_view();
        if (old_inverse_projection != new_inverse_projection || old_inverse_view != new_inverse_view) {
            old_inverse_projection = new_inverse_projection;
            old_inverse_view = new_inverse_view;
            should_clear = true;
            samples = 1;
        } else {
            samples++;
        }

        renderer.begin();
        auto commandbuffer = renderer.current_commandbuffer();
        auto [viewport, scissor] = gfx::helper::fill_viewport_and_scissor_structs(width, height);
        
        if (glfwGetKey(window, GLFW_KEY_C) || should_clear) {
            clear_pass.render(commandbuffer);
            samples = 1;
        }

        raytracer_pass->update_uniform(camera.inverse_projection(), camera.inverse_view(), width, height, triangles.size(), samples, gpu_buffer_triangles, gpu_buffer_materials, gpu_buffer_bvh);
        raytracer_pass->render(commandbuffer);

        horizon_info("{} samples taken", samples);

        renderer.end();
    }

    context.wait_idle();

    return 0;
}

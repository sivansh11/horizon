#include "core/core.hpp"
#include "core/window.hpp"
#include "core/bvh.hpp"
#include "core/random.hpp"
#include "core/ecs.hpp"

#include "gfx/context.hpp"
#include "gfx/helper.hpp"
#include "gfx/base_renderer.hpp"

#include "utility.hpp"
#include "renderer.hpp"
// #include "new_bvh.hpp"

#include <imgui.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

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

struct triangle_t {
    core::vertex_t v0, v1, v2;
    uint32_t material_id;
};

// TODO: remove name from triangle callback
std::pair<core::bvh::bvh_t, std::vector<triangle_t>> bvh_from_model(const core::model_t& model, auto mesh_callback, auto triangle_callback) {
    std::vector<triangle_t> tris;
    std::vector<core::aabb_t> aabbs;
    std::vector<core::vec3> centers;
    for (auto& mesh : model.meshes) {
        mesh_callback(mesh);
        horizon_info("{}", mesh.name);
        for (uint32_t i = 0; i < mesh.indices.size(); i += 3) {
            triangle_t tri {
                mesh.vertices[mesh.indices[i + 0]],
                mesh.vertices[mesh.indices[i + 1]],
                mesh.vertices[mesh.indices[i + 2]],
                0,
            };

            triangle_callback(tri, mesh.name);

            core::aabb_t aabb{};
            aabb.grow(tri.v0.position).grow(tri.v1.position).grow(tri.v2.position);
            core::vec3 center{};
            center = (tri.v0.position + tri.v1.position + tri.v2.position) / 3.f;

            tris.push_back(tri);
            aabbs.push_back(aabb);
            centers.push_back(center);
        }
    }  
    core::bvh::options_t options {
        .o_min_primitive_count         = 1,
        .o_max_primitive_count         = std::numeric_limits<uint32_t>::max(),
        .o_object_split_search_type    = core::bvh::object_split_search_type_t::e_binned_sah,
        .o_primitive_intersection_cost = 1.1f,
        .o_node_intersection_cost      = 1.f,
        .o_samples                     = 100,
    };
    core::bvh::bvh_t bvh = core::bvh::build_bvh2(aabbs, centers, aabbs.size(), options);
    horizon_info("cost of bvh: {}", core::bvh::cost_of_node(bvh, options));
    return { bvh, tris };
}

struct gpu_node_t {
    core::vec3   min;
    core::vec3   max;
    uint32_t     primitive_count;
    uint32_t     first_index;
};

struct gpu_bvh_t {
    VkDeviceAddress p_nodes;
    VkDeviceAddress p_triangles_indices;
    uint32_t node_count;
    uint32_t primitive_count;
};

struct gpu_blas_instance_t {
    VkDeviceAddress p_bvh;
    VkDeviceAddress p_triangless;
    core::vec3 min, max;
    core::mat4 inverse_transform;
};

std::vector<gpu_node_t> simplify_nodes(const std::vector<core::bvh::node_t>& nodes) {
    std::vector<gpu_node_t> simplified_nodes;
    for (auto& node : nodes) {
        gpu_node_t gpu_node{};
        gpu_node.min = node.aabb.min;
        gpu_node.max = node.aabb.max;
        gpu_node.primitive_count = node.primitive_count;
        gpu_node.first_index = node.as.leaf.first_primitive_index;
        simplified_nodes.push_back(gpu_node);
    }

    return simplified_nodes;
}

gpu_buffer_t to_gpu(renderer::base_renderer_t& renderer, const core::bvh::bvh_t& bvh) {
    gpu_bvh_t gpu_bvh;
    horizon_info("sizes: {} {}", bvh.nodes.size(), bvh.primitive_indices.size());
    gpu_bvh.node_count = bvh.nodes.size();
    gpu_bvh.primitive_count = bvh.primitive_indices.size();
    std::vector<gpu_node_t> simplified_nodes = simplify_nodes(bvh.nodes);
    gpu_bvh.p_nodes = to_gpu(renderer, bvh.nodes.data(), sizeof(gpu_node_t) * simplified_nodes.size());
    gpu_bvh.p_triangles_indices = to_gpu(renderer, bvh.primitive_indices.data(), sizeof(uint32_t) * bvh.primitive_indices.size());
    return to_gpu(renderer, &gpu_bvh, sizeof(gpu_bvh));
}

struct material_t {
    uint32_t material_type;
    glm::vec3 albedo;
    float ri_or_fuzz;
};

auto calculate_aabbs_centers_and_triangles_from_model(const core::model_t& model) {
    struct {    
        std::vector<core::aabb_t>     aabbs;
        std::vector<core::vec3>       centers;
        std::vector<triangle_t> triangles;
    } out;

    for (auto& mesh : model.meshes) {
        for (uint32_t i = 0; i < mesh.indices.size(); i += 3) {
            triangle_t triangle {
                mesh.vertices[mesh.indices[i + 0]],
                mesh.vertices[mesh.indices[i + 1]],
                mesh.vertices[mesh.indices[i + 2]],
            };
            core::aabb_t aabb{};
            aabb.grow(triangle.v0.position).grow(triangle.v1.position).grow(triangle.v2.position);
            core::vec3 center = (triangle.v0.position + triangle.v1.position + triangle.v2.position) / 3.f;

            out.triangles.push_back(triangle);
            out.aabbs.push_back(aabb);
            out.centers.push_back(center);
        }
    }
    return out;
}

std::vector<triangle_t> load(std::string path) {
    core::binary_reader_t binary_reader(path);
    std::vector<triangle_t> tris;
    uint32_t count;
    binary_reader.read(count);
    std::cout << count << '\n';
    for (uint32_t i = 0; i < count; i++) {
        triangle_t tri;
        binary_reader.read(tri.v0.position);
        binary_reader.read(tri.v0.normal);
        binary_reader.read(tri.v1.position);
        binary_reader.read(tri.v1.normal);
        binary_reader.read(tri.v2.position);
        binary_reader.read(tri.v2.normal);
        tris.push_back(tri);
    }
    return tris;
}

struct blas_instance_t {
    static blas_instance_t create(core::bvh::bvh_t *p_bvh, triangle_t *p_triangles, const core::mat4& transform) {
        blas_instance_t blas_instance{};
        blas_instance.p_bvh = p_bvh;
        blas_instance.p_triangles = p_triangles;
        blas_instance.aabb = {};

        core::aabb_t root_aabb = p_bvh->nodes[0].aabb;

        for (uint32_t i = 0; i < 8; i++) {
            core::vec3 pos = {
                i & 1 ? root_aabb.max.x : root_aabb.min.x,
                i & 2 ? root_aabb.max.y : root_aabb.min.y,
                i & 4 ? root_aabb.max.z : root_aabb.min.z,
            };
            pos = transform * core::vec4(pos, 1);
            blas_instance.aabb.grow(pos);
        }
        blas_instance.inverse_transform = core::inverse(transform);
        return blas_instance;
    }

    core::bvh::bvh_t *p_bvh;
    triangle_t *p_triangles;
    core::aabb_t aabb;
    core::mat4 inverse_transform;
};


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

    static const uint32_t material_type_lambertian = 0;
    static const uint32_t material_type_diffuse_light = 1;
    static const uint32_t material_type_metal = 2;
    static const uint32_t material_type_dielectric = 3;


    core::rng_t rng{ 0 };

    std::vector<material_t> materials{
        material_t{ material_type_diffuse_light, { 1, 1, 1 } },
        material_t{ material_type_lambertian   , { 1, 1, 1 } },
        material_t{ material_type_lambertian   , { 0, 1, 0 } },
        material_t{ material_type_lambertian   , { 1, 0, 0 } },
        material_t{ material_type_metal        , { 0.91, 0.91, 0.91 }, 0.01 },
        material_t{ material_type_dielectric   , { 1, 1, 1 }, 1.5 },
    };

    for (int i = 0; i < 100; i++) {
        material_t material{ material_type_lambertian };
        material.albedo = rng.random3f();
        materials.push_back(material);
    }

    core::scene_t scene;

    auto id = scene.create();
    {
        scene.construct<core::model_t>(id) = core::load_model_from_path("../../../bvh/assets/model/sponza/sponza.obj");
        auto& transform = scene.construct<transform_t>(id);
        transform.scale = { 0.01, 0.01, 0.01 };
        core::rng_t rng{};
        uint32_t counter = 0;
        auto [bvh, tris] = bvh_from_model(scene.get<core::model_t>(id), [&](auto& mesh) {
            counter++;
        }, [&](auto& tri, std::string name) {
            tri.material_id = counter + 11;
            // tri.material_id = int(rng.randomf() * 456) % 10 + 10;
        });
        // auto [aabbs, centers, tris] = calculate_aabbs_centers_and_triangles_from_model(scene.get<core::model_t>(id));
        scene.construct<std::vector<triangle_t>>(id) = tris;
        scene.construct<core::bvh::bvh_t>(id) = bvh;
    }

    
    std::vector<blas_instance_t> blas_instances;
    std::vector<core::aabb_t> aabbs;
    std::vector<core::vec3> centers;
    scene.for_all<core::bvh::bvh_t, std::vector<triangle_t>, transform_t>([&](auto id, auto& bvh, auto& tris, transform_t& transform) {
        blas_instance_t blas_instance = blas_instance_t::create(&bvh, tris.data(), transform.mat4());
        aabbs.push_back(blas_instance.aabb);
        centers.push_back(blas_instance.aabb.center());
        blas_instances.push_back(blas_instance);
    });
    core::bvh::options_t options {
        .o_min_primitive_count         = 1,
        .o_max_primitive_count         = std::numeric_limits<uint32_t>::max(),
        .o_object_split_search_type    = core::bvh::object_split_search_type_t::e_binned_sah,
        .o_primitive_intersection_cost = 1.1f,
        .o_node_intersection_cost      = 1.f,
        .o_samples                     = 8,
    };
    horizon_info("{}", blas_instances.size());
    core::bvh::bvh_t tlas = core::bvh::build_bvh2(aabbs, centers, blas_instances.size(), options);
    horizon_info("tlas cost: {}", core::bvh::cost_of_node(tlas, options));

    gpu_buffer_t gpu_buffer_tlas = to_gpu(renderer, tlas);

    horizon_info("uploading blas");
    std::vector<gpu_blas_instance_t> gpu_blas_instances;
    for (auto blas_instance : blas_instances) {
        horizon_info("uploading blas instance");
        gpu_blas_instance_t gpu_blas_instance{};
        gpu_blas_instance.inverse_transform = blas_instance.inverse_transform;
        gpu_blas_instance.min = blas_instance.aabb.min;
        gpu_blas_instance.max = blas_instance.aabb.max;
        gpu_blas_instance.p_bvh = to_gpu(renderer, *blas_instance.p_bvh);
        gpu_blas_instance.p_triangless = to_gpu(renderer, blas_instance.p_triangles, blas_instance.p_bvh->primitive_indices.size() * sizeof(triangle_t));
        gpu_blas_instances.push_back(gpu_blas_instance);
    }
    
    horizon_info("uploading tlas");
    gpu_buffer_t gpu_buffer_blas_instances = to_gpu(renderer, gpu_blas_instances.data(), gpu_blas_instances.size() * sizeof(gpu_blas_instance_t));
    gpu_buffer_t gpu_buffer_materials = to_gpu(renderer, materials.data(), materials.size() * sizeof(material_t));

    editor_camera_t camera{ window };
    glm::mat4 old_inverse_projection = camera.inverse_projection();
    glm::mat4 old_inverse_view = camera.inverse_view();

    float target_fps = 10000000.f;
    auto last_time = std::chrono::system_clock::now();

    core::frame_timer_t frame_timer{ 60.f };

    uint32_t samples = 1;

    horizon_info("strarting");

    while (!window.should_close()) {
        core::clear_frame_function_times();
        core::window_t::poll_events();

        bool rebuild_tlas = false;
        bool should_clear = false;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE)) break;
        if (glfwGetKey(window, GLFW_KEY_R)) {
            context.wait_idle();
            raytracer_pass = std::make_shared<raytracer_pass_t>(window, renderer);
            raytracer_pass->update_target_image(sampler, target_image, target_image_view);
            should_clear = true;
        }

        // if (glfwGetKey(window, GLFW_KEY_I)) {
        //     rebuild_tlas = true;
        //     auto& transform = scene.get<transform_t>(id2);
        //     transform.translation.y += 0.1;
        // }

        // if (glfwGetKey(window, GLFW_KEY_Y)) {
        //     rebuild_tlas = true;
        //     auto& transform = scene.get<transform_t>(id2);
        //     transform.translation.y -= 0.1;
        // }

        // if (glfwGetKey(window, GLFW_KEY_K)) {
        //     rebuild_tlas = true;
        //     auto& transform = scene.get<transform_t>(id2);
        //     transform.translation.x += 0.1;
        // }

        // if (glfwGetKey(window, GLFW_KEY_H)) {
        //     rebuild_tlas = true;
        //     auto& transform = scene.get<transform_t>(id2);
        //     transform.translation.x -= 0.1;
        // }

        // if (glfwGetKey(window, GLFW_KEY_U)) {
        //     rebuild_tlas = true;
        //     auto& transform = scene.get<transform_t>(id2);
        //     transform.translation.z += 0.1;
        // }

        // if (glfwGetKey(window, GLFW_KEY_J)) {
        //     rebuild_tlas = true;
        //     auto& transform = scene.get<transform_t>(id2);
        //     transform.translation.z -= 0.1;
        // }
        
        if (rebuild_tlas) {
            rebuild_tlas = false;
            context.wait_idle();
            context.destroy_buffer(gpu_buffer_tlas.handle);
            std::vector<blas_instance_t> blas_instances;
            std::vector<core::aabb_t> aabbs;
            std::vector<core::vec3> centers;
            scene.for_all<core::bvh::bvh_t, std::vector<triangle_t>, transform_t>([&](auto id, auto& bvh, auto& tris, auto& transform) {
                blas_instance_t blas_instance = blas_instance_t::create(&bvh, tris.data(), transform.mat4());
                aabbs.push_back(blas_instance.aabb);
                centers.push_back(blas_instance.aabb.center());
                blas_instances.push_back(blas_instance);
            });
            core::bvh::options_t options {
                .o_min_primitive_count         = 1,
                .o_max_primitive_count         = std::numeric_limits<uint32_t>::max(),
                .o_object_split_search_type    = core::bvh::object_split_search_type_t::e_binned_sah,
                .o_primitive_intersection_cost = 1.1f,
                .o_node_intersection_cost      = 1.f,
                .o_samples                     = 100,
            };
            core::bvh::bvh_t tlas = core::bvh::build_bvh2(aabbs, centers, blas_instances.size(), options);

            gpu_buffer_t gpu_buffer_tlas = to_gpu(renderer, tlas);

            std::vector<gpu_blas_instance_t> gpu_blas_instances;
            for (auto blas_instance : blas_instances) {
                gpu_blas_instance_t gpu_blas_instance{};
                gpu_blas_instance.inverse_transform = blas_instance.inverse_transform;
                gpu_blas_instance.min = blas_instance.aabb.min;
                gpu_blas_instance.max = blas_instance.aabb.max;
                gpu_blas_instance.p_bvh = to_gpu(renderer, *blas_instance.p_bvh);
                gpu_blas_instance.p_triangless = to_gpu(renderer, blas_instance.p_triangles, blas_instance.p_bvh->primitive_indices.size() * sizeof(triangle_t));
                gpu_blas_instances.push_back(gpu_blas_instance);
            }
            gpu_buffer_blas_instances = to_gpu(renderer, gpu_blas_instances.data(), gpu_blas_instances.size() * sizeof(gpu_blas_instance_t));
            should_clear = true;
        }

        auto current_time = std::chrono::system_clock::now();
        auto time_difference = current_time - last_time;
        if (time_difference.count() / 1e6 < 1000.f / target_fps) {
            continue;
        }
        last_time = current_time;

        auto dt = frame_timer.update();
        camera.update(dt.count());

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

        raytracer_pass->update_uniform(camera.inverse_projection(), camera.inverse_view(), width, height, blas_instances.size(), samples, gpu_buffer_blas_instances, gpu_buffer_materials, gpu_buffer_tlas);
        raytracer_pass->render(commandbuffer);

        static int i = 0;
        i++;
        if (i == 25) {
            i = 0;
            horizon_info("{} samples taken", samples);
        } 

        renderer.end();
    }

    context.wait_idle();

    return 0;
}

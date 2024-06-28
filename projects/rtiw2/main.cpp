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

#include <imgui.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

static core::bvh_t::build_options_t build_options{
    .primitive_intersection_cost = 1.1f,
    .node_intersection_cost = 1.f,
    .min_primitive_count = 1,
    .max_primitive_count = std::numeric_limits<uint32_t>::max(),
    .samples = 100,
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

struct triangle_t {
    core::vertex_t v0, v1, v2;
    uint32_t material_id;
};


// struct mesh_t {
//     static mesh_t process_mesh(core::mesh_t mesh) {
//         core::rng_t rng;
//         mesh_t r_mesh{};
//         std::vector<core::aabb_t> aabbs;
//         std::vector<core::vec3> centers;
//         for (uint32_t i = 0; i < mesh.indices.size(); i += 3) {
//             triangle_t tri {
//                 mesh.vertices[mesh.indices[i + 0]],
//                 mesh.vertices[mesh.indices[i + 1]],
//                 mesh.vertices[mesh.indices[i + 2]],
//                 0,
//             };

//             tri.material_id = int(rng.randomf() * 23456788.f) % 10;

//             core::aabb_t aabb{};
//             aabb.grow(tri.v0.position).grow(tri.v1.position).grow(tri.v2.position);
//             core::vec3 center{};
//             center = (tri.v0.position + tri.v1.position + tri.v2.position) / 3.f;

//             r_mesh.tris.push_back(tri);
//             aabbs.push_back(aabb);
//             centers.push_back(center);
//         }
//         r_mesh.bvh = core::bvh_t::construct(aabbs.data(), centers.data(), r_mesh.tris.size(), build_options);
//         return r_mesh;
//     }
//     core::bvh_t bvh;
//     std::vector<triangle_t> tris;
// };

// struct model_t {
//     static model_t process_model(core::model_t model) {
//         horizon_info("model has {} meshes", model.meshes.size());
//         model_t r_model{};
//         for (auto& mesh : model.meshes) {
//             r_model.meshes.push_back(mesh_t::process_mesh(mesh));
//         }
//         return r_model;
//     }
//     std::vector<mesh_t> meshes;
// };

// struct scene_t {
//     void add_model(const model_t& model, const core::mat4& transform) {
//         models.push_back(model);
//         transformations.push_back(transform);
//     }

//     std::pair<core::bvh_t, std::vector<core::blas_instance_t<triangle_t>>> create_tlas() {
//         std::vector<core::blas_instance_t<triangle_t>> blas_instances{};
//         for (uint32_t i = 0; i < models.size(); i++) {
//             model_t& model = models[i];
//             core::mat4& transform = transformations[i];

//             for (auto& mesh : model.meshes) {
//                 core::blas_instance_t<triangle_t> blas_instance{ &mesh.bvh, mesh.tris.data(), transform };
//                 blas_instances.push_back(blas_instance);
//             }
//         }

//         std::vector<core::aabb_t> aabbs;
//         std::vector<core::vec3> centers;
//         for (auto& blas_instance : blas_instances) {
//             aabbs.push_back(blas_instance.aabb);
//             centers.push_back(blas_instance.aabb.center());
//         }
        
//         core::bvh_t tlas = core::bvh_t::construct(aabbs.data(), centers.data(), blas_instances.size(), build_options);
//         return { tlas, blas_instances };
//     }

//     std::vector<model_t> models;
//     std::vector<core::mat4> transformations;
// };

std::pair<core::bvh_t, std::vector<triangle_t>> bvh_from_model(const core::model_t& model, auto mesh_callback, auto triangle_callback) {
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
    core::bvh_t bvh = core::bvh_t::construct(aabbs.data(), centers.data(), tris.size(), build_options);
    return { bvh, tris };
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

struct gpu_blas_instance_t {
    VkDeviceAddress p_bvh;
    VkDeviceAddress p_primitives;
    core::vec3 min, max;
    core::mat4 inverse_transform;
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

    horizon_info("\e[0;97mprimitive count: {}", bvh.primitive_count);
    horizon_info("\e[0;97mnode count: {}", bvh.node_count);
    horizon_info("\e[0;97mdepth: {}", bvh.depth());
    horizon_info("\e[0;97mglobal sah cost: {}", bvh.node_traversal_cost(build_options));
    horizon_info("\e[0;97mleaf count: {}", leaf_node_count);
    horizon_info("\e[0;97mmax tris per leaf: {}", max);
    horizon_info("\e[0;97mmin tris per leaf: {}", min);
    horizon_info("\e[0;97maverage tris per leaf: {}", average / float(leaf_node_count));
}


struct material_t {
    uint32_t material_type;
    glm::vec3 albedo;
    float ri_or_fuzz;
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

    // auto id1 = scene.create();
    // {
    //     scene.construct<core::model_t>(id1) = core::load_model_from_path("../../assets/models/corenl_box.obj");
    //     auto& transform = scene.construct<transform_t>(id1);
    //     auto [bvh, tris] = bvh_from_model(scene.get<core::model_t>(id1), [](auto& tri, std::string name) {
    //         if (name == "area_light") {
    //             tri.material_id = 0;
    //         }

    //         if (name == "back_wall") {
    //             tri.material_id = 4;
    //         }

    //         if (name == "ceiling") {
    //             tri.material_id = 1;
    //         }

    //         if (name == "floor") {
    //             tri.material_id = 1;
    //         }

    //         if (name == "left_wall") {
    //             tri.material_id = 2;
    //         }

    //         if (name == "right_wall") {
    //             tri.material_id = 3;
    //         }

    //         if (name == "short_box") {
    //             tri.material_id = 1;
    //         }

    //         if (name == "tall_box") {
    //             // tri.material_id = 5;
    //             tri.material_id = 1;
    //         }
            
    //     });
    //     scene.construct<std::vector<triangle_t>>(id1) = tris;
    //     scene.construct<core::bvh_t>(id1) = bvh;
    // }

    // auto id2 = scene.create();
    // {
    //     scene.construct<core::model_t>(id2) = core::load_model_from_path("../../assets/models/dragon.obj");
    //     auto& transform = scene.construct<transform_t>(id2);
    //     // transform.scale = { 0.1, 0.1, 0.1 };
    //     transform.translation = { 0.69, 1.80, -1.8 };
    //     transform.rotation = { 0, glm::pi<float>() + glm::half_pi<float>(), 0 };
    //     auto [bvh, tris] = bvh_from_model(scene.get<core::model_t>(id2), [](auto& tri, std::string name) {
    //         tri.material_id = 5;
    //     });
    //     scene.construct<std::vector<triangle_t>>(id2) = tris;
    //     scene.construct<core::bvh_t>(id2) = bvh;
    // }

    auto id3 = scene.create();
    {
        scene.construct<core::model_t>(id3) = core::load_model_from_path("../../assets/models/glTF-Sample-Models/2.0/Sponza/glTF/Sponza.gltf");
        auto& transform = scene.construct<transform_t>(id3);
        transform.scale = { 0.01, 0.01, 0.01 };
        core::rng_t rng{};
        uint32_t counter = 0;
        auto [bvh, tris] = bvh_from_model(scene.get<core::model_t>(id3), [&](auto& mesh) {
            counter++;
        }, [&](auto& tri, std::string name) {
            tri.material_id = counter + 11;
            // tri.material_id = int(rng.randomf() * 456) % 10 + 10;
        });
        scene.construct<std::vector<triangle_t>>(id3) = tris;
        scene.construct<core::bvh_t>(id3) = bvh;
    }

    auto id4 = scene.create();
    {
        scene.construct<core::model_t>(id4) = core::load_model_from_path("../../assets/models/glTF-Sample-Models/1.0/Box/glTF/Box.gltf");
        auto& transform = scene.construct<transform_t>(id4);
        // transform.scale = { 0.01, 0.01, 0.01 };
        transform.translation = { 0, 5, 0, };
        core::rng_t rng{};
        uint32_t counter = 0;
        auto [bvh, tris] = bvh_from_model(scene.get<core::model_t>(id4), [](auto& mesh) {}, [&](auto& tri, std::string name) {
            tri.material_id = 0;
        });
        scene.construct<std::vector<triangle_t>>(id4) = tris;
        scene.construct<core::bvh_t>(id4) = bvh;
    }


    std::vector<core::blas_instance_t<triangle_t>> blas_instances;
    std::vector<core::aabb_t> aabbs;
    std::vector<core::vec3> centers;
    scene.for_all<core::bvh_t, std::vector<triangle_t>, transform_t>([&](auto id, auto& bvh, auto& tris, auto& transform) {
        core::blas_instance_t<triangle_t> blas_instance(&bvh, tris.data(), transform.mat4());
        aabbs.push_back(blas_instance.aabb);
        centers.push_back(blas_instance.aabb.center());
        blas_instances.push_back(blas_instance);
    });
    
    core::bvh_t tlas = core::bvh_t::construct(aabbs.data(), centers.data(), blas_instances.size(), build_options);

    gpu_buffer_t gpu_buffer_tlas = to_gpu(renderer, tlas);

    std::vector<gpu_blas_instance_t> gpu_blas_instances;
    for (auto blas_instance : blas_instances) {
        gpu_blas_instance_t gpu_blas_instance{};
        gpu_blas_instance.inverse_transform = blas_instance.inverse_transform;
        gpu_blas_instance.min = blas_instance.aabb.min;
        gpu_blas_instance.max = blas_instance.aabb.max;
        gpu_blas_instance.p_bvh = to_gpu(renderer, *blas_instance.bvh);
        gpu_blas_instance.p_primitives = to_gpu(renderer, blas_instance.primitives, blas_instance.bvh->primitive_count * sizeof(triangle_t));
        gpu_blas_instances.push_back(gpu_blas_instance);
    }

    gpu_buffer_t gpu_buffer_blas_instances = to_gpu(renderer, gpu_blas_instances.data(), gpu_blas_instances.size() * sizeof(gpu_blas_instance_t));
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
            std::vector<core::blas_instance_t<triangle_t>> blas_instances;
            std::vector<core::aabb_t> aabbs;
            std::vector<core::vec3> centers;
            scene.for_all<core::bvh_t, std::vector<triangle_t>, transform_t>([&](auto id, auto& bvh, auto& tris, auto& transform) {
                core::blas_instance_t<triangle_t> blas_instance(&bvh, tris.data(), transform.mat4());
                aabbs.push_back(blas_instance.aabb);
                centers.push_back(blas_instance.aabb.center());
                blas_instances.push_back(blas_instance);
            });
            core::bvh_t tlas = core::bvh_t::construct(aabbs.data(), centers.data(), blas_instances.size(), build_options);
            gpu_buffer_tlas = to_gpu(renderer, tlas);
            gpu_blas_instances.clear();
            for (auto blas_instance : blas_instances) {
                gpu_blas_instance_t gpu_blas_instance{};
                gpu_blas_instance.inverse_transform = blas_instance.inverse_transform;
                gpu_blas_instance.min = blas_instance.aabb.min;
                gpu_blas_instance.max = blas_instance.aabb.max;
                gpu_blas_instance.p_bvh = to_gpu(renderer, *blas_instance.bvh);
                gpu_blas_instance.p_primitives = to_gpu(renderer, blas_instance.primitives, blas_instance.bvh->primitive_count * sizeof(triangle_t));
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

        horizon_info("{} samples taken", samples);

        renderer.end();
    }

    context.wait_idle();

    return 0;
}

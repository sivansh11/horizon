#include "core/model.hpp"
#include "core/ecs.hpp"
#include "utility.hpp"
#include "core/bvh.hpp"

static const core::bvh_t::build_options_t build_options{
    .primitive_intersection_cost = 1.1f,
    .node_intersection_cost = 1.f,
    .min_primitive_count = 1,
    .max_primitive_count = std::numeric_limits<uint32_t>::max(),
    .samples = 100,
    .add_node_intersection_cost_in_leaf_traversal = false,
};

struct triangle_t {
    core::vertex_t v0, v1, v2;
};

// struct mesh_t {
//     static mesh_t process_mesh(core::mesh_t mesh) {
//         mesh_t r_mesh{};
//         std::vector<core::aabb_t> aabbs;
//         std::vector<core::vec3> centers;
//         for (uint32_t i = 0; i < mesh.indices.size(); i += 3) {
//             triangle_t tri {
//                 mesh.vertices[mesh.indices[i + 0]],
//                 mesh.vertices[mesh.indices[i + 1]],
//                 mesh.vertices[mesh.indices[i + 2]],
//             };

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

std::pair<core::bvh_t, std::vector<triangle_t>> bvh_from_model(const core::model_t& model) {
    std::vector<triangle_t> tris;
    std::vector<core::aabb_t> aabbs;
    std::vector<core::vec3> centers;
    for (auto& mesh : model.meshes) {
        for (uint32_t i = 0; i < mesh.indices.size(); i += 3) {
            triangle_t tri {
                mesh.vertices[mesh.indices[i + 0]],
                mesh.vertices[mesh.indices[i + 1]],
                mesh.vertices[mesh.indices[i + 2]],
            };

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

int main() {

    core::scene_t scene;
    // {
    //     auto id = scene.create();
        
    //     scene.construct<core::model_t>(id) = core::load_model_from_path("../../assets/models/glTF-Sample-Models/2.0/Sponza/glTF/Sponza.gltf");
        
    //     auto& transform = scene.construct<transform_t>(id);
    //     transform.scale = { 0.01, 0.01, 0.01 };

    //     auto [bvh, tris] = bvh_from_model(scene.get<core::model_t>(id));
    //     scene.construct<std::vector<triangle_t>>(id) = tris;
    //     scene.construct<core::bvh_t>(id) = bvh;

    //     auto& blas_instance = scene.construct<core::blas_instance_t<triangle_t>>(id, &scene.get<core::bvh_t>(id), scene.get<std::vector<triangle_t>>(id).data(), transform.mat4());

    // }

    {
        auto id = scene.create();
        
        scene.construct<core::model_t>(id) = core::load_model_from_path("../../assets/models/corenl_box.obj");
        
        auto& transform = scene.construct<transform_t>(id);

        auto [bvh, tris] = bvh_from_model(scene.get<core::model_t>(id));
        scene.construct<std::vector<triangle_t>>(id) = tris;
        scene.construct<core::bvh_t>(id) = bvh;

        auto& blas_instance = scene.construct<core::blas_instance_t<triangle_t>>(id, &scene.get<core::bvh_t>(id), scene.get<std::vector<triangle_t>>(id).data(), transform.mat4());

    }

    {
        auto id = scene.create();
        
        scene.construct<core::model_t>(id) = core::load_model_from_path("../../assets/models/teapot.obj");
        
        auto& transform = scene.construct<transform_t>(id);

        auto [bvh, tris] = bvh_from_model(scene.get<core::model_t>(id));
        scene.construct<std::vector<triangle_t>>(id) = tris;
        scene.construct<core::bvh_t>(id) = bvh;

        auto& blas_instance = scene.construct<core::blas_instance_t<triangle_t>>(id, &scene.get<core::bvh_t>(id), scene.get<std::vector<triangle_t>>(id).data(), transform.mat4());

    }


    std::vector<core::blas_instance_t<triangle_t>> blas_instances;
    std::vector<core::aabb_t> aabbs;
    std::vector<core::vec3> centers;
    scene.for_all<core::blas_instance_t<triangle_t>>([&](auto id, auto& blas_instance) {
        aabbs.push_back(blas_instance.aabb);
        centers.push_back(blas_instance.aabb.center());
        blas_instances.push_back(blas_instance);
    });
    
    core::bvh_t tlas = core::bvh_t::construct(aabbs.data(), centers.data(), blas_instances.size(), build_options);

    // scene_t scene{};
    // core::mat4 transform = core::scale(core::mat4{ 1.f }, { 1, 1, 1 });
    // scene.add_model(model_t::process_model(core::load_model_from_path("../../assets/models/sponza_bbk/SponzaMerged/SponzaMerged.obj")), transform);



    return 0;
}
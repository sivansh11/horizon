#include "core/model.hpp"

#include "bvh.hpp"

struct triangle_t {
    core::vertex_t p0, p1, p2;
};

struct mesh_t {
    uint32_t primitive_offset;
    uint32_t primitive_count;
    horizon::bvh_t bvh;
};

struct model_t {
    std::vector<mesh_t> meshes;
};

model_t create_model_from_raw_model(const core::model_t& raw_model, std::vector<triangle_t>& global_triangle_pool) {
    model_t model{};
    for (auto& raw_mesh : raw_model.meshes) {
        mesh_t mesh{};
        std::vector<horizon::aabb_t> aabbs;
        std::vector<glm::vec3> centers;
        mesh.primitive_offset = global_triangle_pool.size();
        mesh.primitive_count = raw_mesh.indices.size() / 3;
        for (uint32_t i = 0; i < raw_mesh.indices.size(); i+=3) {
            triangle_t triangle {
                raw_mesh.vertices[raw_mesh.indices[i + 0]],
                raw_mesh.vertices[raw_mesh.indices[i + 1]],
                raw_mesh.vertices[raw_mesh.indices[i + 2]],
            };
            global_triangle_pool.push_back(triangle);
            horizon::aabb_t aabb{};
            aabb.extend(triangle.p0.position).extend(triangle.p1.position).extend(triangle.p2.position);
            aabbs.push_back(aabb);
            glm::vec3 center = (triangle.p0.position + triangle.p1.position + triangle.p2.position) / 3.f;
            centers.push_back(center);
        }
        mesh.bvh = horizon::bvh_t{ aabbs.data(), centers.data(), mesh.primitive_count };
        model.meshes.push_back(mesh);
    }
    return model;
}

struct scene_t {
    void add_model(const model_t& model, const glm::mat4& transform) {
        models.push_back(model);
        transforms.push_back(transform);
    }

    horizon::bvh_t create_tlas(const std::vector<triangle_t>& global_triangle_pool) {
        // std::vector<
    }

    std::vector<model_t> models;
    std::vector<glm::mat4> transforms;
};

int main() {
    std::vector<triangle_t> global_triangle_pool;
    
    scene_t scene;

    auto cornel_box_model = create_model_from_raw_model(core::load_model_from_path("../../assets/models/corenl_box.obj"), global_triangle_pool);


    return 0;
}
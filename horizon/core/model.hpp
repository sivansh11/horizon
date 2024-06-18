#ifndef CORE_MODEL_HPP
#define CORE_MODEL_HPP

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <vector>
#include <filesystem>

namespace core {

struct vertex_t {
    glm::vec3 position{};
    glm::vec3 normal{};
    glm::vec2 uv{};
    glm::vec3 tangent{};
    glm::vec3 bi_tangent{};
};

enum texture_type_t {
    e_diffuse_map,
    e_specular_map,
    e_normal_map,
    e_diffuse_color,
};

struct texture_info_t {
    texture_type_t texture_type{};
    std::filesystem::path file_path{};
    glm::vec4 diffuse_color{};
};

struct material_description_t {
    std::vector<texture_info_t> texture_infos{};
};

// struct triangle_t {
//     glm::vec3 center() { return (v0.position + v1.position + v2.position) / 3.f; }
//     vertex_t v0{}, v1{}, v2{};
// };

const float infinity = std::numeric_limits<float>::max();

struct aabb_t {
    aabb_t& grow(const glm::vec3& point) {
        min = glm::min(min, point);
        max = glm::max(max, point);
        return *this;
    }

    aabb_t& grow(const aabb_t& aabb) {
        min = glm::min(min, aabb.min);
        max = glm::max(max, aabb.max);
        return *this;
    }

    // aabb_t& grow(const triangle_t& triangle) {
    //     grow(triangle.v0.position).grow(triangle.v1.position).grow(triangle.v2.position);
    //     return *this;
    // }

    float half_area() const {
        glm::vec3 e = max - min;
        return e.x * e.y + e.y * e.z + e.z * e.x;
    }

    glm::vec3 center() const {
        return (max + min) / 2.f;
    }

    glm::vec3 min{ infinity }, max{ -infinity };
};

struct mesh_t {
    std::vector<vertex_t> vertices{};
    std::vector<uint32_t> indices{}; 
    material_description_t material_description{};
    aabb_t aabb{};
    std::string name{};
};

struct model_t {
    std::vector<mesh_t> meshes;
};

struct model_loading_info_t {
    std::filesystem::path file_path;
    model_t model;
};

model_t load_model_from_path(const std::filesystem::path& file_path);

} // namespace core

#endif
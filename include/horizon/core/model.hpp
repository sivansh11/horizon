#ifndef CORE_MODEL_HPP
#define CORE_MODEL_HPP

#include "aabb.hpp"

#include <filesystem>
#include <vector>

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
//     glm::vec3 center() { return (v0.position + v1.position + v2.position)
//     / 3.f; } vertex_t v0{}, v1{}, v2{};
// };

struct raw_mesh_t {
  std::vector<vertex_t> vertices{};
  std::vector<uint32_t> indices{};
  material_description_t material_description{};
  aabb_t aabb{};
  std::string name{};
};

struct raw_model_t {
  std::vector<raw_mesh_t> meshes;
};

struct model_loading_info_t {
  std::filesystem::path file_path;
  raw_model_t model;
};

raw_model_t load_model_from_path(const std::filesystem::path &file_path);

} // namespace core

#endif

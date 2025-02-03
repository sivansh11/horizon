#include <limits>
#include <stack>
#include <vector>

#include "horizon/core/aabb.hpp"
#include "horizon/core/core.hpp"
#include "horizon/core/logger.hpp"
#include "horizon/core/math.hpp"
#include "horizon/core/model.hpp"

#include "bvh.hpp"
#include "image.hpp"

struct triangle_t {
  uint32_t i0, i1, i2;
};

struct bvh_triangle_t {
  core::vec3 p0, p1, p2;
};

auto extract_aabb_triangles_from_model(core::raw_model_t &model) {
  std::vector<core::aabb_t> aabbs{};
  std::vector<bvh_triangle_t> bvh_triangles{};
  std::vector<triangle_t> triangles{};
  std::vector<core::vertex_t> vertices{};

  uint32_t index_offset = 0;
  for (auto &mesh : model.meshes) {
    for (uint32_t i = 0; i < mesh.indices.size(); i += 3) {
      bvh_triangle_t bvh_triangle{
          mesh.vertices[mesh.indices[i + 0]].position,
          mesh.vertices[mesh.indices[i + 1]].position,
          mesh.vertices[mesh.indices[i + 2]].position,
      };
      // TODO: figure out how to handle triangle
      core::aabb_t aabb{};
      aabb.grow(bvh_triangle.p0).grow(bvh_triangle.p1).grow(bvh_triangle.p2);
      core::vec3 center{};

      bvh_triangles.push_back(bvh_triangle);
      aabbs.push_back(aabb);
    }
  }
  return std::tuple{aabbs, bvh_triangles};
}

struct ray_t {
  core::vec3 o, d;
  core::vec3 id;
  float tmin, tmax;
};

ray_t create_ray(core::vec3 o, core::vec3 d) {
  ray_t ray{};

  ray.o = o;
  ray.d = d;
  ray.tmin = std::numeric_limits<uint32_t>::epsilon();
  ray.tmax = infinity;
  ray.id = {
      core::safe_inverse(d.x),
      core::safe_inverse(d.y),
      core::safe_inverse(d.z),
  };
  return ray;
}

struct camera_t {
  static camera_t create(uint32_t width, uint32_t height, float vfov,
                         core::vec3 from, core::vec3 at,
                         core::vec3 up = core::vec3{0, 1, 0}) {
    camera_t camera{};
    camera.width = width;
    camera.height = height;
    camera.vfov = vfov;
    camera.from = from;
    camera.at = at;
    camera.up = up;

    float aspect_ratio = float(width) / float(height);
    float focal_length = length(from - at);
    float theta = radians(vfov);
    float h = tan(theta / 2.f);
    float viewport_height = 2.f * h * focal_length;
    float viewport_width = viewport_height * aspect_ratio;

    camera.w = normalize(from - at);
    camera.u = normalize(cross(up, camera.w));
    camera.v = cross(camera.w, camera.u);

    core::vec3 viewport_u = viewport_width * camera.u;
    core::vec3 viewport_v = viewport_height * -camera.v;

    camera.pixel_delta_u = viewport_u / float(width);
    camera.pixel_delta_v = viewport_v / float(height);

    core::vec3 viewport_upper_left =
        from - (focal_length * camera.w) - viewport_u / 2.f - viewport_v / 2.f;
    camera.pixel_00_loc = viewport_upper_left +
                          0.5f * (camera.pixel_delta_u + camera.pixel_delta_v);
    return camera;
  }

  ray_t ray_gen(uint32_t x, uint32_t y) {
    core::vec3 pixel_center =
        pixel_00_loc + (float(x) * pixel_delta_u) + (float(y) * pixel_delta_v);
    core::vec3 direction = pixel_center - from;
    return create_ray(from, direction);
  }

  uint32_t width, height;
  float vfov;
  core::vec3 from, at, up;

private:
  core::vec3 pixel_00_loc, pixel_delta_u, pixel_delta_v, u, v, w;
};

struct triangle_intersection_t {
  bool did_intersect() { return is_intersect; }
  bool is_intersect;
  f32 t, u, v, w;
};

struct aabb_intersection_t {
  bool did_intersect() { return tmin <= tmax; }
  f32 tmin, tmax;
};

constexpr uint32_t invalid_index = std::numeric_limits<uint32_t>::max();

struct hit_t {
  bool did_intersect() { return primitive_index != invalid_index; }
  uint32_t primitive_index;
  f32 t = infinity;
  f32 u, v, w;
};

inline triangle_intersection_t
triangle_intersect(const ray_t &ray, const bvh_triangle_t &triangle) {
  vec3 e1 = triangle.p0 - triangle.p1;
  vec3 e2 = triangle.p2 - triangle.p0;
  vec3 n = cross(e1, e2);

  vec3 c = triangle.p0 - ray.o;
  vec3 r = cross(ray.d, c);
  float inverse_det = 1.0f / dot(n, ray.d);

  float u = dot(r, e2) * inverse_det;
  float v = dot(r, e1) * inverse_det;
  float w = 1.0f - u - v;

  triangle_intersection_t intersection;

  if (u >= 0 && v >= 0 && w >= 0) {
    float t = dot(n, c) * inverse_det;
    if (t >= ray.tmin && t <= ray.tmax) {
      intersection.is_intersect = true;
      intersection.t = t;
      intersection.u = u;
      intersection.v = v;
      intersection.w = w;
      return intersection;
    }
  }
  intersection.is_intersect = false;
  return intersection;
}

inline aabb_intersection_t aabb_intersect(const aabb_t &aabb,
                                          const ray_t &ray) {
  vec3 tmin = (aabb.min - ray.o) * ray.id;
  vec3 tmax = (aabb.max - ray.o) * ray.id;

  const vec3 old_tmin = tmin;
  const vec3 old_tmax = tmax;

  tmin = min(old_tmin, old_tmax);
  tmax = max(old_tmin, old_tmax);

  float _tmin = max(tmin[0], max(tmin[1], max(tmin[2], ray.tmin)));
  float _tmax = min(tmax[0], min(tmax[1], min(tmax[2], ray.tmax)));

  aabb_intersection_t aabb_intersection = {_tmin, _tmax};
  return aabb_intersection;
}

static constexpr hit_t null_hit{.primitive_index = invalid_index};
static constexpr u32 STACK_SIZE = 16;

inline hit_t traverse(bvh::bvh_t &bvh, ray_t &ray, bvh_triangle_t *triangles) {
  hit_t hit = null_hit;

  std::stack<uint32_t> stack{};
  stack.push(0);

  while (stack.size()) {
    auto node_index = stack.top();
    stack.pop();
    auto &node = bvh.nodes[node_index];
    if (aabb_intersect(node.aabb, ray).did_intersect()) {
      if (node.is_leaf) {
        for (auto &refrence : std::span{
                 bvh.refrences.begin() + node.as.leaf.first_reference_index,
                 bvh.refrences.begin() + node.as.leaf.first_reference_index +
                     node.refrence_count}) {
          if (aabb_intersect(refrence.aabb, ray).did_intersect()) {
            triangle_intersection_t ti =
                triangle_intersect(ray, triangles[refrence.primitive_index]);
            if (ti.did_intersect() && hit.t > ti.t) {
              hit.t = ti.t;
              hit.u = ti.u;
              hit.v = ti.v;
              hit.w = ti.w;
              hit.primitive_index = refrence.primitive_index;
              ray.tmax = ti.t;
            }
          }
        }
      } else {
        for (uint32_t i = node.as.internal.first_child_index;
             i < node.as.internal.first_child_index +
                     node.as.internal.children_count;
             i++) {
          stack.push(i);
        }
      }
    }
  }

  return hit;
}

vec4 color(uint32_t id) {
  vec4 col = vec4(((id * 9665) % 256) / 255.f, ((id * 8765976) % 256) / 255.f,
                  ((id * 2345678) % 256) / 255.f, 1);
  return col;
}

int main() {

  auto model = core::load_model_from_path(
      "../../../horizon_cpy/assets/models/corenl_box.obj");
  // "../../../horizon_cpy/assets/models/sponza_bbk/SponzaMerged/"
  // "SponzaMerged.obj");
  // "../../../horizon_cpy/assets/models/teapot.obj");

  auto [aabbs, bvh_triangles] = extract_aabb_triangles_from_model(model);

  bvh::options_t o{};

  bvh::bvh_t bvh = bvh::build_bvh2(aabbs.data(), bvh_triangles.size(), o);

  horizon_info("built bvh");

  image_t image{640, 420};
  camera_t camera = camera_t::create(640, 420, -45, {0, 2.5, 10}, {0, 2.5,
  0});
  // camera_t camera = camera_t::create(640, 420, -45, {0, 2, 0}, {1, 2, 0});

  for (int32_t j = image.height - 1; j >= 0; j--)
    for (int32_t i = image.width - 1; i >= 0; i--) {
      auto ray = camera.ray_gen(i, j);
      auto hit = traverse(bvh, ray, bvh_triangles.data());
      if (hit.did_intersect()) {
        image.at(i, j) = color(hit.primitive_index);
      } else {
        image.at(i, j) = vec4{0, 0, 0, 0};
      }
    }

  image.to_disk("test.ppm");

  return 0;
}

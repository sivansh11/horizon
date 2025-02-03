#include "horizon/core/bvh.hpp"
#include "horizon/core/math.hpp"
#include "horizon/core/model.hpp"

#include <filesystem>
#include <fstream>
#include <limits>

using namespace core;

class image_t {
public:
  image_t(u32 width, u32 height) : _width(width), _height(height) {
    _p_pixel = new glm::vec4[_width * _height];
  }

  ~image_t() { delete[] _p_pixel; }

  vec4 &at(u32 x, u32 y) { return _p_pixel[y * _width + x]; }

  void to_disk(const std::filesystem::path &path) {
    std::stringstream s;
    s << "P3\n" << _width << ' ' << _height << "\n255\n";
    for (i32 j = _height - 1; j >= 0; j--)
      for (i32 i = 0; i < _width; i++) {
        vec4 pixel = at(i, j);
        s << u32(clamp(pixel.r, 0.f, 1.f) * 255) << ' '
          << u32(clamp(pixel.g, 0.f, 1.f) * 255) << ' '
          << u32(clamp(pixel.b, 0.f, 1.f) * 255) << '\n';
      }

    std::ofstream file{path};
    if (!file.is_open()) {
      throw std::runtime_error("Failed to open file");
    }
    file << s.str();
    file.close();
  }

  template <typename T> T clamp(T val, T min, T max) {
    return val > max ? max : val < min ? min : val;
  }
  const u32 _width, _height;
  vec4 *_p_pixel{nullptr};
};

struct triangle_t {
  vertex_t v0, v1, v2;
};

inline f32 safe_inverse(f32 x) {
  static constexpr f32 epsilon = std::numeric_limits<f32>::epsilon();
  if (std::abs(x) <= epsilon) {
    return x >= 0 ? 1.f / epsilon : -1.f / epsilon;
  }
  return 1.f / x;
}

struct ray_t {
  static ray_t create(vec3 origin, vec3 direction) {
    return {.origin = origin, .direction = direction};
  }
  vec3 origin, direction;
};

struct ray_data_t : public ray_t {
  static ray_data_t create(const ray_t &ray) {
    ray_data_t ray_data{};
    ray_data.origin = ray.origin;
    ray_data.direction = ray.direction;
    ray_data.tmin = std::numeric_limits<f32>::epsilon();
    ray_data.tmax = infinity;
    ray_data.inverse_direction = vec3{
        safe_inverse(ray.direction.x),
        safe_inverse(ray.direction.y),
        safe_inverse(ray.direction.z),
    };
    return ray_data;
  }

  vec3 inverse_direction;
  f32 tmin, tmax;
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

struct hit_t {
  bool did_intersect() { return primitive_index != bvh::invalid_index; }
  uint32_t primitive_index;
  f32 t = infinity;
  f32 u, v, w;
};

struct camera_t {
  core::mat4 inverse_projection;
  core::mat4 inverse_view;
};

class editor_camera_t {
public:
  void update_projection(float aspect_ratio);
  void update(float ts, float aspect);

  const core::mat4 &projection() const { return _projection; }
  const core::mat4 &view() const { return _view; }
  // creates a new mat4
  core::mat4 inverse_projection() const { return core::inverse(_projection); }
  // creates a new mat4
  core::mat4 inverse_view() const { return core::inverse(_view); }
  core::vec3 front() const { return _front; }
  core::vec3 right() const { return _right; }
  core::vec3 up() const { return _up; }
  core::vec3 position() const { return _position; }
  ray_data_t ray_gen(vec2 uv) {
    vec2 px_nds = uv * 2.f - 1.f;
    vec3 point_nds = vec3(px_nds, -1);
    vec4 point_ndsh = vec4(point_nds, 1);
    vec4 dir_eye = point_ndsh * inverse_projection();
    // vec4 dir_eye = inverse_projection() * point_ndsh;
    dir_eye.w = 0;
    vec3 dir_world = vec3(dir_eye * inverse_view());
    // vec3 dir_world = vec3(inverse_view() * dir_eye);
    vec3 eye = {inverse_view()[3][0], inverse_view()[3][1],
                inverse_view()[3][2]};
    // vec3 eye = { inverse_view()[0][3], inverse_view()[1][3],
    // inverse_view()[2][3] };
    return ray_data_t::create(ray_t::create(eye, dir_world));
  }

public:
  float fov{90.0f}; // zoom var ?
  float camera_speed_multiplyer{1};

  float far{1000.f};
  float near{0.1f};

private:
  core::vec3 _position{0, 0, 0};

  core::mat4 _projection{1.0f};
  core::mat4 _view{1.0f};

  core::vec3 _front{0, 0, 1};
  core::vec3 _up{0, 1, 0};
  core::vec3 _right{1, 0, 0};

  core::vec2 _initial_mouse{};

  float _yaw{0};
  float _pitch{0};
  float _mouse_speed{0.005f};
  float _mouse_sensitivity{100.f};
};

void editor_camera_t::update_projection(float aspect_ratio) {
  static float s_aspect_ratio = 0;
  if (s_aspect_ratio != aspect_ratio) {
    _projection =
        core::perspective(core::radians(fov), aspect_ratio, near, far);
    s_aspect_ratio = aspect_ratio;
  }
}

void editor_camera_t::update(float ts, float aspect) {
  update_projection(aspect);

  core::vec3 front = _front;
  _front = front * camera_speed_multiplyer;
  _right = core::normalize(core::cross(_front, core::vec3{0, 1, 0}));
  _up = core::normalize(core::cross(_right, _front));

  _view = core::lookAt(_position, _position + _front, core::vec3{0, 1, 0});
}
static constexpr hit_t null_hit{.primitive_index = bvh::invalid_index};
static constexpr u32 STACK_SIZE = 16;

inline triangle_intersection_t triangle_intersect(const ray_data_t &ray,
                                                  const triangle_t &triangle) {
  vec3 e1 = triangle.v0.position - triangle.v1.position;
  vec3 e2 = triangle.v2.position - triangle.v0.position;
  vec3 n = cross(e1, e2);

  vec3 c = triangle.v0.position - ray.origin;
  vec3 r = cross(ray.direction, c);
  float inverse_det = 1.0f / dot(n, ray.direction);

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
                                          const ray_data_t &ray) {
  vec3 tmin = (aabb.min - ray.origin) * ray.inverse_direction;
  vec3 tmax = (aabb.max - ray.origin) * ray.inverse_direction;

  const vec3 old_tmin = tmin;
  const vec3 old_tmax = tmax;

  tmin = min(old_tmin, old_tmax);
  tmax = max(old_tmin, old_tmax);

  float _tmin = max(tmin[0], max(tmin[1], max(tmin[2], ray.tmin)));
  float _tmax = min(tmax[0], min(tmax[1], min(tmax[2], ray.tmax)));

  aabb_intersection_t aabb_intersection = {_tmin, _tmax};
  return aabb_intersection;
}

inline hit_t intersect(const bvh::bvh_t &bvh, ray_data_t &ray,
                       triangle_t *p_triangles) {
  hit_t hit = null_hit;

  u32 stack[STACK_SIZE];
  u32 stack_top = 0;

  bvh::node_t root = bvh.nodes[0];
  if (!aabb_intersect(root.aabb, ray).did_intersect())
    return hit;

  if (root.is_leaf) {
    for (u32 i = 0; i < root.primitive_count; i++) {
      u32 primitive_index =
          bvh.primitive_indices[root.as.leaf.first_primitive_index + i];
      triangle_t &triangle = p_triangles[primitive_index];
      triangle_intersection_t intersection = triangle_intersect(ray, triangle);
      if (intersection.did_intersect()) {
        ray.tmax = intersection.t;
        hit.primitive_index = primitive_index;
        hit.t = intersection.t;
        hit.u = intersection.u;
        hit.v = intersection.v;
        hit.w = intersection.w;
      }
    }
    return hit;
  }

  u32 current = 1;
  while (true) {
    const bvh::node_t &left = bvh.nodes[current];
    const bvh::node_t &right = bvh.nodes[current + 1];

    aabb_intersection_t left_intersect = aabb_intersect(left.aabb, ray);
    aabb_intersection_t right_intersect = aabb_intersect(right.aabb, ray);

    u32 start = 0;
    u32 end = 0;
    if (left_intersect.did_intersect() && left.is_leaf) {
      if (right_intersect.did_intersect() && right.is_leaf) {
        start = left.as.leaf.first_primitive_index;
        end = right.as.leaf.first_primitive_index + right.primitive_count;
      } else {
        start = left.as.leaf.first_primitive_index;
        end = left.as.leaf.first_primitive_index + left.primitive_count;
      }
    } else if (right_intersect.did_intersect() && right.is_leaf) {
      start = right.as.leaf.first_primitive_index;
      end = right.as.leaf.first_primitive_index + right.primitive_count;
    }
    for (u32 i = start; i < end; i++) {
      const u32 primitive_index = bvh.primitive_indices[i];
      triangle_t &triangle = p_triangles[primitive_index];
      triangle_intersection_t intersection = triangle_intersect(ray, triangle);
      if (intersection.did_intersect()) {
        ray.tmax = intersection.t;
        hit.primitive_index = primitive_index;
        hit.t = intersection.t;
        hit.u = intersection.u;
        hit.v = intersection.v;
        hit.w = intersection.w;
      }
    }

    if (left_intersect.did_intersect() && !left.is_leaf) {
      if (right_intersect.did_intersect() && !right.is_leaf) {
        if (stack_top >= STACK_SIZE)
          return hit; // TODO: maybe raise an error ?
        if (left_intersect.tmin <= right_intersect.tmin) {
          current = left.as.internal.first_child_index;
          stack[stack_top++] = right.as.internal.first_child_index;
        } else {
          current = right.as.internal.first_child_index;
          stack[stack_top++] = left.as.internal.first_child_index;
        }
      } else {
        current = left.as.internal.first_child_index;
      }
    } else {
      if (right_intersect.did_intersect() && !right.is_leaf) {
        current = right.as.internal.first_child_index;
      } else {
        if (stack_top == 0)
          return hit;
        current = stack[--stack_top];
      }
    }
  }
}

vec4 color(uint32_t id) {
  auto col = vec4{((id * 9665) % 256) / 255.f, ((id * 8765976) % 256) / 255.f,
                  ((id * 2345678) % 256) / 255.f, 1};
  return col;
}

int main() {
  std::vector<aabb_t> aabbs;
  std::vector<vec3> centers;
  std::vector<triangle_t> triangles;

  triangle_t triangle{
      {core::vec3{-0.5, -0.5, 2}},
      {core::vec3{0.5, -0.5, 2}},
      {core::vec3{0, 0.5, 2}},
  };
  core::aabb_t aabb{};
  aabb.grow(triangle.v0.position)
      .grow(triangle.v1.position)
      .grow(triangle.v2.position);
  core::vec3 center{};
  center =
      (triangle.v0.position + triangle.v1.position + triangle.v2.position) /
      3.f;
  aabbs.push_back(aabb);
  triangles.push_back(triangle);
  aabbs.push_back(aabb);

  //  model_t model =
  // load_model_from_path("../../../horizon_cpy/assets/models/teapot.obj");
  //
  //  for (auto mesh : model.meshes) {
  //    for (uint32_t i = 0; i < mesh.indices.size(); i+=3) {
  //      triangle_t triangle{
  //        .v0 = mesh.vertices[mesh.indices[i + 0]],
  //        .v1 = mesh.vertices[mesh.indices[i + 1]],
  //        .v2 = mesh.vertices[mesh.indices[i + 2]],
  //      };
  //      aabb_t aabb{};
  //      aabb.grow(triangle.v0.position).grow(triangle.v1.position).grow(triangle.v2.position);
  //      vec3 center{};
  //      center = (triangle.v0.position + triangle.v1.position +
  // triangle.v2.position) / 3.f;
  //
  //      triangles.push_back(triangle);
  //      aabbs.push_back(aabb);
  //      centers.push_back(center);
  //    }
  //  }
  //
  bvh::options_t options{
      .o_min_primitive_count = 1, // TODO: try 0
      .o_max_primitive_count = std::numeric_limits<u32>::max(),
      .o_object_split_search_type =
          bvh::object_split_search_type_t::e_binned_sah,
      .o_primitive_intersection_cost = 1.1f,
      .o_node_intersection_cost = 1.f,
      .o_samples = 8,
  };

  auto bvh =
      bvh::build_bvh2(aabbs.data(), centers.data(), triangles.size(), options);

  editor_camera_t editor_camera{};
  editor_camera.update(0.01, float(640) / float(420));
  image_t image{640, 420};

  for (u32 i = 0; i < image._width; i++)
    for (u32 j = 0; j < image._height; j++) {
      ray_data_t ray = editor_camera.ray_gen(vec2(
          float(i) / float(image._width), float(j) / float(image._height)));
      auto hit = intersect(bvh, ray, triangles.data());
      if (hit.did_intersect()) {
        image.at(i, j) = color(hit.primitive_index);
      } else {
        image.at(i, j) = vec4{0, 0, 0, 1};
      }
    }

  image.to_disk("test.ppm");
  return 0;
}

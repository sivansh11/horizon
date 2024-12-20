#include "horizon/core/aabb.hpp"
#include "horizon/core/bvh.hpp"
#include "horizon/core/math.hpp"
#include "horizon/core/model.hpp"

#include "horizon/gfx/context.hpp"
#include "horizon/gfx/helper.hpp"

#include <cstring>
#include <vulkan/vulkan_core.h>

struct ray_t {
  core::vec3 origin;
  core::vec3 direction;
};

struct triangle_t {
  core::vec3 v0, v1, v2;
};

inline float safe_inverse(float x) {
  static constexpr float epsilon = std::numeric_limits<float>::epsilon();
  if (std::abs(x) <= epsilon) {
    return x >= 0 ? 1.f / epsilon : -1.f / epsilon;
  }
  return 1.f / x;
}

struct ray_data_t : public ray_t {
  static ray_data_t create(const ray_t &ray) {
    ray_data_t ray_data{};
    ray_data.origin = ray.origin;
    ray_data.direction = ray.direction;
    ray_data.tmin = std::numeric_limits<float>::epsilon();
    ray_data.tmax = core::infinity;
    ray_data.inverse_direction = core::vec3{
        safe_inverse(ray.direction.x),
        safe_inverse(ray.direction.y),
        safe_inverse(ray.direction.z),
    };
    return ray_data;
  }

  core::vec3 inverse_direction;
  float tmin, tmax;
};

struct triangle_intersection_t {
  bool did_intersect() { return is_intersect; }
  bool is_intersect;
  float t, u, v, w;
};

struct aabb_intersection_t {
  bool did_intersect() { return tmin <= tmax; }
  float tmin, tmax;
};

struct hit_t {
  bool did_intersect() { return primitive_id != core::bvh::invalid_index; }
  uint32_t primitive_id;
  float t = core::infinity;
  float u, v, w;
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
  ray_data_t ray_gen(core::vec2 uv) {
    core::vec2 px_nds = uv * 2.f - 1.f;
    core::vec3 point_nds = core::vec3(px_nds, -1);
    core::vec4 point_ndsh = core::vec4(point_nds, 1);
    core::vec4 dir_eye = point_ndsh * inverse_projection();
    // core::vec4 dir_eye = inverse_projection() * point_ndsh;
    dir_eye.w = 0;
    core::vec3 dir_world = core::vec3(dir_eye * inverse_view());
    // core::vec3 dir_world = core::vec3(inverse_view() * dir_eye);
    core::vec3 eye = {inverse_view()[3][0], inverse_view()[3][1],
                      inverse_view()[3][2]};
    // core::vec3 eye = { inverse_view()[0][3], inverse_view()[1][3],
    // inverse_view()[2][3] };
    return ray_data_t::create(ray_t(eye, dir_world));
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
static constexpr hit_t null_hit{.primitive_id = core::bvh::invalid_index};
static constexpr uint32_t STACK_SIZE = 16;

inline triangle_intersection_t triangle_intersect(const ray_data_t &ray,
                                                  const triangle_t &triangle) {
  core::vec3 e1 = triangle.v0 - triangle.v1;
  core::vec3 e2 = triangle.v2 - triangle.v0;
  core::vec3 n = cross(e1, e2);

  core::vec3 c = triangle.v0 - ray.origin;
  core::vec3 r = cross(ray.direction, c);
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

inline aabb_intersection_t aabb_intersect(const ray_data_t &ray,
                                          const core::aabb_t &aabb) {
  core::vec3 tmin = (aabb.min - ray.origin) * ray.inverse_direction;
  core::vec3 tmax = (aabb.max - ray.origin) * ray.inverse_direction;

  const core::vec3 old_tmin = tmin;
  const core::vec3 old_tmax = tmax;

  tmin = min(old_tmin, old_tmax);
  tmax = max(old_tmin, old_tmax);

  float _tmin =
      core::max(tmin[0], core::max(tmin[1], core::max(tmin[2], ray.tmin)));
  float _tmax =
      core::min(tmax[0], core::min(tmax[1], core::min(tmax[2], ray.tmax)));

  aabb_intersection_t aabb_intersection = {_tmin, _tmax};
  return aabb_intersection;
}

inline hit_t intersect(const core::bvh::bvh_t &bvh, ray_data_t &ray,
                       triangle_t *p_triangles) {
  hit_t hit = null_hit;

  uint32_t stack[STACK_SIZE];
  uint32_t stack_top = 0;

  core::bvh::node_t root = bvh.nodes[0];
  if (!aabb_intersect(ray, root.aabb).did_intersect())
    return hit;

  if (root.is_leaf) {
    for (uint32_t i = 0; i < root.primitive_count; i++) {
      uint32_t primitive_index =
          bvh.primitive_indices[root.as.leaf.first_primitive_index + i];
      triangle_t &triangle = p_triangles[primitive_index];
      triangle_intersection_t intersection = triangle_intersect(ray, triangle);
      if (intersection.did_intersect()) {
        ray.tmax = intersection.t;
        hit.primitive_id = primitive_index;
        hit.t = intersection.t;
        hit.u = intersection.u;
        hit.v = intersection.v;
        hit.w = intersection.w;
      }
    }
    return hit;
  }

  uint32_t current = 1;
  while (true) {
    const core::bvh::node_t &left = bvh.nodes[current];
    const core::bvh::node_t &right = bvh.nodes[current + 1];

    aabb_intersection_t left_intersect = aabb_intersect(ray, left.aabb);
    aabb_intersection_t right_intersect = aabb_intersect(ray, right.aabb);

    uint32_t start = 0;
    uint32_t end = 0;
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
    for (uint32_t i = start; i < end; i++) {
      const uint32_t primitive_index = bvh.primitive_indices[i];
      triangle_t &triangle = p_triangles[primitive_index];
      triangle_intersection_t intersection = triangle_intersect(ray, triangle);
      if (intersection.did_intersect()) {
        ray.tmax = intersection.t;
        hit.primitive_id = primitive_index;
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

gfx::handle_buffer_t create_test_buffer(gfx::context_t &ctx, size_t size) {
  gfx::config_buffer_t cb{};
  cb.vk_size = size;
  cb.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  cb.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
  return ctx.create_buffer(cb);
}

void test_node(gfx::context_t &ctx) {
  core::bvh::node_t node{
    .aabb = core::aabb_t{ .min = {1, 2, 3}, .max={4, 5, 6} },
    .is_leaf = true,
    .primitive_count = 7,
  };
  node.as.internal.first_child_index = 8;
  node.as.internal.children_count = 9;

  auto node_buffer = create_test_buffer(ctx, sizeof(core::bvh::node_t));

  struct push_constant_t {
    VkDeviceAddress node;
  } pc{};

  pc.node = ctx.get_buffer_device_address(node_buffer);

  gfx::config_shader_t cs{};
  cs.name = "test_node.slang";
  cs.type = gfx::shader_type_t::e_compute;
  cs.is_code = false;
  cs.language = gfx::shader_language_t::e_slang;
  cs.code_or_path = "../../assets/shaders/bvh_test_gpu/test_node.slang";
  gfx::handle_shader_t s = ctx.create_shader(cs);

  gfx::config_pipeline_layout_t cpl{};
  cpl.add_push_constant(sizeof(push_constant_t), VK_SHADER_STAGE_ALL);
  gfx::handle_pipeline_layout_t pl = ctx.create_pipeline_layout(cpl);

  gfx::config_pipeline_t cp{};
  cp.handle_pipeline_layout = pl;
  cp.add_shader(s);
  gfx::handle_pipeline_t p = ctx.create_compute_pipeline(cp);

  auto command_pool = ctx.create_command_pool({});

  std::memcpy(ctx.map_buffer(node_buffer), &node, sizeof(core::bvh::node_t));

  auto cbuf = gfx::helper::begin_single_use_commandbuffer(ctx, command_pool);
  ctx.cmd_bind_pipeline(cbuf, p);
  ctx.cmd_push_constants(cbuf, p, VK_SHADER_STAGE_ALL, 0,
                         sizeof(push_constant_t), &pc);
  ctx.cmd_dispatch(cbuf, 1, 1, 1);
  gfx::helper::end_single_use_command_buffer(ctx, cbuf);
}

void test_triangle(gfx::context_t &ctx) {
  ray_t ray{
      .origin = core::vec3{0, 0, -1},
      .direction = core::vec3{0, 0, 1},
  };
  triangle_t triangle{
      .v0 = core::vec3{-0.5, -0.5, 0},
      .v1 = core::vec3{0.5, -0.5, 0},
      .v2 = core::vec3{0, 0.5, 0},
  };

  ray_data_t ray_data = ray_data_t::create(ray);
  uint32_t res = 0;
  if (triangle_intersect(ray_data, triangle).did_intersect()) {
    res = 1;
  }

  auto ray_buffer = create_test_buffer(ctx, sizeof(ray_t));
  auto triangle_buffer = create_test_buffer(ctx, sizeof(triangle_t));
  auto res_buffer = create_test_buffer(
      ctx,
      std::max(sizeof(uint32_t), std::max(sizeof(triangle_t), sizeof(ray_t))));

  gfx::config_shader_t cs{};
  cs.name = "test_triangle.slang";
  cs.type = gfx::shader_type_t::e_compute;
  cs.is_code = false;
  cs.language = gfx::shader_language_t::e_slang;
  cs.code_or_path = "../../assets/shaders/bvh_test_gpu/test_triangle.slang";
  gfx::handle_shader_t s = ctx.create_shader(cs);

  gfx::config_pipeline_layout_t cpl{};
  cpl.add_push_constant(3 * sizeof(VkDeviceSize), VK_SHADER_STAGE_ALL);
  gfx::handle_pipeline_layout_t pl = ctx.create_pipeline_layout(cpl);

  gfx::config_pipeline_t cp{};
  cp.handle_pipeline_layout = pl;
  cp.add_shader(s);
  gfx::handle_pipeline_t p = ctx.create_compute_pipeline(cp);

  auto command_pool = ctx.create_command_pool({});

  std::memcpy(ctx.map_buffer(ray_buffer), &ray, sizeof(ray_t));
  std::memcpy(ctx.map_buffer(triangle_buffer), &triangle, sizeof(triangle_t));

  struct push_constant_t {
    VkDeviceAddress ray;
    VkDeviceAddress triangle;
    VkDeviceAddress res;
  } pc{};

  pc.ray = ctx.get_buffer_device_address(ray_buffer);
  pc.triangle = ctx.get_buffer_device_address(triangle_buffer);
  pc.res = ctx.get_buffer_device_address(res_buffer);

  auto cbuf = gfx::helper::begin_single_use_commandbuffer(ctx, command_pool);
  ctx.cmd_bind_pipeline(cbuf, p);
  ctx.cmd_push_constants(cbuf, p, VK_SHADER_STAGE_ALL, 0,
                         3 * sizeof(VkDeviceAddress), &pc);
  ctx.cmd_dispatch(cbuf, 1, 1, 1);
  gfx::helper::end_single_use_command_buffer(ctx, cbuf);

  check(*reinterpret_cast<uint32_t *>(ctx.map_buffer(res_buffer)) == res,
        "res and res_buffer is not equivalent");
}

void test_aabb(gfx::context_t &ctx) {
  ray_t ray{
      .origin = core::vec3{0, 0, -1},
      .direction = core::vec3{0, 0, 1},
  };
  core::aabb_t aabb = core::aabb_t{.min = {-1, -1, 0}, .max = {1, 1, 0}};

  ray_data_t ray_data = ray_data_t::create(ray);
  uint32_t res = 0;
  if (aabb_intersect(ray_data, aabb).did_intersect()) {
    res = 1;
  }

  auto ray_buffer = create_test_buffer(ctx, sizeof(ray_t));
  auto aabb_buffer = create_test_buffer(ctx, sizeof(core::aabb_t));
  auto res_buffer = create_test_buffer(
      ctx, std::max(sizeof(uint32_t),
                    std::max(sizeof(core::aabb_t), sizeof(ray_t))));

  gfx::config_shader_t cs{};
  cs.name = "test_aabb.slang";
  cs.type = gfx::shader_type_t::e_compute;
  cs.is_code = false;
  cs.language = gfx::shader_language_t::e_slang;
  cs.code_or_path = "../../assets/shaders/bvh_test_gpu/test_aabb.slang";
  gfx::handle_shader_t s = ctx.create_shader(cs);

  gfx::config_pipeline_layout_t cpl{};
  cpl.add_push_constant(3 * sizeof(VkDeviceSize), VK_SHADER_STAGE_ALL);
  gfx::handle_pipeline_layout_t pl = ctx.create_pipeline_layout(cpl);

  gfx::config_pipeline_t cp{};
  cp.handle_pipeline_layout = pl;
  cp.add_shader(s);
  gfx::handle_pipeline_t p = ctx.create_compute_pipeline(cp);

  auto command_pool = ctx.create_command_pool({});

  std::memcpy(ctx.map_buffer(ray_buffer), &ray, sizeof(ray_t));
  std::memcpy(ctx.map_buffer(aabb_buffer), &aabb, sizeof(core::aabb_t));

  struct push_constant_t {
    VkDeviceAddress ray;
    VkDeviceAddress aabb;
    VkDeviceAddress res;
  } pc{};

  pc.ray = ctx.get_buffer_device_address(ray_buffer);
  pc.aabb = ctx.get_buffer_device_address(aabb_buffer);
  pc.res = ctx.get_buffer_device_address(res_buffer);

  auto cbuf = gfx::helper::begin_single_use_commandbuffer(ctx, command_pool);
  ctx.cmd_bind_pipeline(cbuf, p);
  ctx.cmd_push_constants(cbuf, p, VK_SHADER_STAGE_ALL, 0,
                         3 * sizeof(VkDeviceAddress), &pc);
  ctx.cmd_dispatch(cbuf, 1, 1, 1);
  gfx::helper::end_single_use_command_buffer(ctx, cbuf);

  check(*reinterpret_cast<uint32_t *>(ctx.map_buffer(res_buffer)) == res,
        "res and res_buffer is not equivalent");
}

int main() {
  gfx::context_t ctx{false};

  test_triangle(ctx);
  test_aabb(ctx);
  test_node(ctx);

  return 0;
}

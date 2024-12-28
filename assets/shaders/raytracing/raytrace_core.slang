public static const float epsilon = 0.0001;
public static const float infinity = 100000000000000.f;

public static const uint32_t invalid_index = 4294967295u;

public static const uint32_t STACK_SIZE = 16;

static const uint32_t work_group_width = 8;
static const uint32_t work_group_height = 8;

public struct aabb_t {
    float3 min = float3(infinity), max = float3(-infinity);
};

#define FIRST_INDEX_BITS_SIZE 28;
public struct node_t {
    aabb_t aabb;
    uint32_t is_leaf : 1;
    uint32_t primitive_count : 31;
    uint32_t first_primitive_index_or_first_child_index : FIRST_INDEX_BITS_SIZE;
    uint32_t children_count: 4;    // do not use
};

public struct bvh_t {
    node_t *nodes;
    uint32_t *primitive_indices;
};

public struct vertex_t {
    float3 position;
    float3 normal;
    float2 uv;
    float3 tangent;
    float3 bi_tangent;
};

public struct triangle_t {
    // float3 v0, v1, v2;
    vertex_t v0, v1, v2;
    vertex_t from_barry(float u, float v, float w) {
        vertex_t vertex;
        vertex.position = u * v0.position + v * v1.position + w * v2.position;
        vertex.normal = u * v0.normal + v * v1.normal + w * v2.normal;
        vertex.uv = u * v0.uv + v * v1.uv + w * v2.uv;
        vertex.tangent = u * v0.tangent + v * v1.tangent + w * v2.tangent;
        vertex.bi_tangent = u * v0.bi_tangent + v * v1.bi_tangent + w * v2.bi_tangent;
        return vertex;
    }
};

public float safe_inverse(float x) {
    static const float epsilon = epsilon;
    if (abs(x) <= epsilon) return x >= 0 ? 1.f / epsilon : -1.f / epsilon;
    return 1.f / x;
}

public struct ray_t {
    float3 origin, direction;
};

public struct ray_data_t {
    static ray_data_t create(ray_t ray, uint32_t px_id) {
        return create(ray.origin, ray.direction, px_id);
    }
    static ray_data_t create(float3 origin, float3 direction, uint32_t px_id) {
        ray_data_t ray_data;
        ray_data.origin = origin;
        ray_data.direction = direction;
        ray_data.inverse_direction = float3(
            safe_inverse(ray_data.direction.x),
            safe_inverse(ray_data.direction.y),
            safe_inverse(ray_data.direction.z),
        );
        ray_data.tmin = epsilon;
        ray_data.tmax = infinity;
        ray_data.px_id = px_id;
        return ray_data;
    }
    float3 origin, direction;
    float3 inverse_direction;
    float tmin, tmax;
    uint32_t px_id;
}

public struct triangle_intersection_t {
    bool did_intersect() { return is_intersect; }
    bool is_intersect;
    float t, u, v, w;
};

public struct aabb_intersection_t {
    bool did_intersect() { return tmin <= tmax; }
    float tmin, tmax;
};

public struct hit_t {
    bool did_intersect() { return primitive_id != invalid_index; }
    uint32_t primitive_id = invalid_index;
    float t = infinity;
    float u = 0, v = 0, w = 0;
    uint32_t node_intersection_count = 0;
    uint32_t primitive_intersection_count = 0;
};

float4 operator *(float4x4 m, float4 v) {
    return mul(m, v);
}

float4 operator *(float4 v, float4x4 m) {
    return mul(v, m);
}

extension float3 {
    __init(float4 v) {
        this.x = v.x;
        this.y = v.y;
        this.z = v.z;
    }
};
                        
public struct camera_t {
    ray_data_t ray_gen(float2 uv, uint32_t px_id) {
        float2 px_nds = uv * 2.f - 1.f;
        float3 point_nds = float3(px_nds, -1);
        float4 point_ndsh = float4(point_nds, 1);
        float4 dir_eye = point_ndsh * inverse_projection;
        dir_eye.w = 0;
        float3 dir_world = float3(dir_eye * inverse_view);
        float3 eye = { inverse_view[3][0], inverse_view[3][1], inverse_view[3][2] };
        return ray_data_t::create(eye, dir_world, px_id);
    }
    float4x4 inverse_projection;
    float4x4 inverse_view;
};

public triangle_intersection_t triangle_intersect(const ray_data_t ray_data, const triangle_t triangle) {
    float3 e1 = triangle.v0.position - triangle.v1.position;
    float3 e2 = triangle.v2.position - triangle.v0.position;
    float3 n = cross(e1, e2);

    float3 c = triangle.v0.position - ray_data.origin;
    float3 r = cross(ray_data.direction, c);
    float inverse_det = 1.0f / dot(n, ray_data.direction);

    float u = dot(r, e2) * inverse_det;
    float v = dot(r, e1) * inverse_det;
    float w = 1.0f - u - v;

    triangle_intersection_t intersection;

    if (u >= 0 && v >= 0 && w >= 0) {
        float t = dot(n, c) * inverse_det;
        if (t >= ray_data.tmin && t <= ray_data.tmax) {
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

public aabb_intersection_t aabb_intersect(const ray_data_t ray_data, const aabb_t aabb) {
    float3 tmin = (aabb.min - ray_data.origin) * ray_data.inverse_direction;
    float3 tmax = (aabb.max - ray_data.origin) * ray_data.inverse_direction;

    const float3 old_tmin = tmin;
    const float3 old_tmax = tmax;

    tmin = min(old_tmin, old_tmax);
    tmax = max(old_tmin, old_tmax);

    float _tmin = max(tmin[0], max(tmin[1], max(tmin[2], ray_data.tmin)));
    float _tmax = min(tmax[0], min(tmax[1], min(tmax[2], ray_data.tmax)));

    aabb_intersection_t aabb_intersection = { _tmin, _tmax };
    return aabb_intersection;
}

groupshared uint32_t stack[work_group_width * work_group_height][STACK_SIZE];

public hit_t intersect(const bvh_t bvh, ray_data_t ray, triangle_t *p_triangles, uint32_t group_index) {
    hit_t hit;
    hit.primitive_id = invalid_index;

    // uint32_t stack[STACK_SIZE];
    uint32_t stack_top = 0;

    hit.node_intersection_count++;

    node_t root = bvh.nodes[0];
    if (!aabb_intersect(ray, root.aabb).did_intersect()) return hit;

    if (bool(root.is_leaf)) {
        for (uint32_t i = 0; i < root.primitive_count; i++) {
            hit.primitive_intersection_count++;
        uint32_t primitive_index = bvh.primitive_indices[root.first_primitive_index_or_first_child_index + i];
        triangle_t triangle = p_triangles[primitive_index];
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
      const node_t left  = bvh.nodes[current];
      const node_t right = bvh.nodes[current + 1];

      hit.node_intersection_count += 2;
      aabb_intersection_t left_intersect = aabb_intersect(ray, left.aabb);
      aabb_intersection_t right_intersect = aabb_intersect(ray, right.aabb);

      uint32_t start = 0;
      uint32_t end = 0;
      if (left_intersect.did_intersect() && bool(left.is_leaf)) {
        if (right_intersect.did_intersect() && bool(right.is_leaf)) {
          start = left.first_primitive_index_or_first_child_index;
          end = right.first_primitive_index_or_first_child_index + right.primitive_count;
        } else {
          start = left.first_primitive_index_or_first_child_index;
          end = left.first_primitive_index_or_first_child_index + left.primitive_count;
        }
      } else if (right_intersect.did_intersect() && bool(right.is_leaf)) {
          start = right.first_primitive_index_or_first_child_index;
          end = right.first_primitive_index_or_first_child_index + right.primitive_count;
      }
      for (uint32_t i = start; i < end; i++) {
          hit.primitive_intersection_count++;
        const uint32_t primitive_index = bvh.primitive_indices[i];
        triangle_t triangle = p_triangles[primitive_index];
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

      if (left_intersect.did_intersect() && !bool(left.is_leaf)) {
        if (right_intersect.did_intersect() && !bool(right.is_leaf)) {
          if (stack_top >= STACK_SIZE) return hit; // TODO: maybe raise an error ?
          if (left_intersect.tmin <= right_intersect.tmin) {
            current = left.first_primitive_index_or_first_child_index;
            // stack[stack_top++] = right.first_primitive_index_or_first_child_index;
            stack[group_index][stack_top++] = right.first_primitive_index_or_first_child_index;
          } else {
            current = right.first_primitive_index_or_first_child_index;
            // stack[stack_top++] = left.first_primitive_index_or_first_child_index;
            stack[group_index][stack_top++] = left.first_primitive_index_or_first_child_index;
          }
        } else {
          current = left.first_primitive_index_or_first_child_index;
        }
      } else {
        if (right_intersect.did_intersect() && !bool(right.is_leaf)) {
          current = right.first_primitive_index_or_first_child_index;
        } else {
          if (stack_top == 0) return hit;
          // current = stack[--stack_top];
          current = stack[group_index][--stack_top];
        }
      }
    }
}

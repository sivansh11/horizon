static const float epsilon = 0.0001;
static const float infinity = 100000000000000.f;

static const uint32_t invalid_index = 4294967295u;

#define FIRST_INDEX_BITS_SIZE 28
#define STACK_SIZE 16

struct aabb_t {
    float3 min = float3(infinity), max = float3(-infinity);
};

struct node_t {
    aabb_t aabb;
    uint32_t is_leaf : 1;
    uint32_t primitive_count : 31;
    uint32_t first_primitive_index_or_first_child_index : FIRST_INDEX_BITS_SIZE;
    uint32_t children_count: 4;    // do not use
};

struct bvh_t {
    node_t *nodes;
    uint32_t *primitive_indices;
};

struct triangle_t {
    float3 v0, v1, v2;
};

float safe_inverse(float x) {
    static const float epsilon = epsilon;
    if (abs(x) <= epsilon) return x >= 0 ? 1.f / epsilon : -1.f / epsilon;
    return 1.f / x;
}

struct ray_data_t {
    static ray_data_t create(float3 origin, float3 direction) {
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
        return ray_data;
    }
    float3 origin, direction;
    float3 inverse_direction;
    float tmin, tmax;
}

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
    bool did_intersect() { return primitive_id != invalid_index; }
    uint32_t primitive_id;
    float t = infinity;
    float u, v, w;
}

struct camera_t {
    ray_data_t ray_gen(uint32_t x, uint32_t y) {
        float3 pixel_center = pixel_00_loc + (float(x) * pixel_delta_u) + (float(y) * pixel_delta_v);
        float3 direction = pixel_center - from;
        return ray_data_t::create(from, direction);
    }
    uint32_t width, height;
    float vfov;
    float3 from, at, up;
    float3 pixel_00_loc, pixel_delta_u, pixel_delta_v, u, v, w;
};

inline triangle_intersection_t triangle_intersect(const ray_data_t ray_data, const triangle_t triangle) {
    float3 e1 = triangle.v0- triangle.v1;
    float3 e2 = triangle.v2- triangle.v0;
    float3 n = cross(e1, e2);

    float3 c = triangle.v0- ray_data.origin;
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

inline aabb_intersection_t aabb_intersect(const aabb_t aabb, const ray_data_t ray_data) {
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

hit_t intersect(const bvh_t bvh, ray_data_t ray, triangle_t *p_triangles) {
    hit_t hit;
    hit.primitive_id = invalid_index;

    uint32_t stack[STACK_SIZE];
    uint32_t stack_top = 0;

    node_t root = bvh.nodes[0];
    if (!aabb_intersect(root.aabb, ray).did_intersect()) return hit;

    if (bool(root.is_leaf)) {
      for (uint32_t i = 0; i < root.primitive_count; i++) {
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

      aabb_intersection_t left_intersect = aabb_intersect(left.aabb, ray);
      aabb_intersection_t right_intersect = aabb_intersect(right.aabb, ray);

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
            stack[stack_top++] = right.first_primitive_index_or_first_child_index;
          } else {
            current = right.first_primitive_index_or_first_child_index;
            stack[stack_top++] = left.first_primitive_index_or_first_child_index;
          }
        } else {
          current = left.first_primitive_index_or_first_child_index;
        }
      } else {
        if (right_intersect.did_intersect() && !bool(right.is_leaf)) {
          current = right.first_primitive_index_or_first_child_index;
        } else {
          if (stack_top == 0) return hit;
          current = stack[--stack_top];
        }
      }
    }
}
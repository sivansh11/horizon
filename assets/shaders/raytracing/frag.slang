import vert;

struct pixel_t {
    float4 color : COLOR0;
};

struct node_t {
    bool is_leaf() { return primitive_count != 0; }
    float3 min;
    uint32_t primitive_count;
    float3 max;
    uint32_t first_index;
};

struct bvh_t {
    node_t *p_nodes;
    uint32_t *p_primitive_indices;
    uint32_t *p_parent_ids;
    uint32_t node_count;
    uint32_t primitive_count;
};

struct triangle_t {
    float3 v0, v1, v2;
};

struct uniform_t {
    bvh_t *bvh;
    triangle_t *triangles;
    float4x4 inverse_projection;
    float4x4 inverse_view;
};

uniform uniform_t uniform;

float safe_inverse(float x) {
    const float epsilon = 0.001;
    if (abs(x) <= epsilon) {
        if (x >= 0) return 1.f / epsilon;
        return -1.f / epsilon;
    }
    return 1.f / x;
}

struct ray_t {
    static ray_t create(float3 origin, float3 direction) {
        ray_t ray;
        ray.origin = origin;
        ray.direction = direction;
        ray.tmin = 0;
        ray.tmax = 10000000;
        ray.inverse_direction = { safe_inverse(direction.x), safe_inverse(direction.y), safe_inverse(direction.z) };
        return ray;
    }

    float3 origin, direction, inverse_direction;
    float tmin, tmax;
};

struct triangle_intersection_t {
    bool did_intersect() { return intersection; }
    bool intersection;
    float t;
};

triangle_intersection_t triangle_intersect(triangle_t triangle, ray_t ray) {
    float3 e1 = triangle.v0 - triangle.v1;
    float3 e2 = triangle.v2 - triangle.v0;
    float3 n = cross(e1, e2);

    float3 c = triangle.v0 - ray.origin;
    float3 r = cross(ray.direction, c);
    float inverse_det = 1.0f / dot(n, ray.direction);

    float u = dot(r, e2) * inverse_det;
    float v = dot(r, e1) * inverse_det;
    float w = 1.0f - u - v;

    if (u >= 0 && v >= 0 && w >= 0) {
        float t = dot(n, c) * inverse_det;
        if (t >= ray.tmin && t <= ray.tmax) {
            // ray.tmax = t;
            triangle_intersection_t intersection;
            intersection.intersection = true;
            intersection.t = t;
            return intersection;
        }
    }
    triangle_intersection_t intersection;
    intersection.intersection = false;
    intersection.t = 100000000;
    return intersection;
}

bool node_intersect(node_t node, ray_t ray) {
    float3 tmin = (node.min - ray.origin) * ray.inverse_direction;
    float3 tmax = (node.max - ray.origin) * ray.inverse_direction;

    const float3 old_tmin = tmin;
    const float3 old_tmax = tmax;

    tmin = min(old_tmin, old_tmax);
    tmax = max(old_tmin, old_tmax);

    float _tmin = max(tmin[0], max(tmin[1], max(tmin[2], ray.tmin)));
    float _tmax = min(tmax[0], min(tmax[1], min(tmax[2], ray.tmax)));

    return _tmin <= _tmax;
}

static const uint32_t null_primitive_id = uint32_t(-1);
struct hit_t {
    bool did_intersect() { return primitive_id != null_primitive_id; }
    uint32_t primitive_id = null_primitive_id;
    float t;
    uint32_t node_intersect_count = 0;
    uint32_t primitive_intersect_count = 0;
    uint32_t loops = 0;
    uint32_t passed = 0;
};

void intersect_leaf_node(in const node_t *node, inout hit_t hit, bvh_t *bvh, inout ray_t ray, triangle_t *triangles) {
    for (uint32_t i = 0; i < node.primitive_count; i++) {
        const uint32_t primitive_id = bvh.p_primitive_indices[node.first_index + i];
        hit.primitive_intersect_count++;
        triangle_intersection_t intersection = triangle_intersect(triangles[primitive_id], ray);
        if (intersection.did_intersect()) {
            ray.tmax = intersection.t;
            hit.primitive_id = primitive_id;
            hit.t = intersection.t;
        }
    }
}

hit_t stack_traverse(bvh_t *bvh, inout ray_t ray, triangle_t *triangles) {
    hit_t hit;

    uint32_t stack[32];
    uint32_t stack_top = 0;

    stack[stack_top++] = 0;

    while (stack_top != 0) {
        hit.loops++;
        node_t *node = &bvh->p_nodes[stack[--stack_top]];

        hit.node_intersect_count++;
        if (!node_intersect(*node, ray)) continue;

        if (node->is_leaf()) {
            intersect_leaf_node(node, hit, bvh, ray, triangles);
        } else {
            stack[stack_top++] = node->first_index;
            stack[stack_top++] = node->first_index + 1;
        }
    }
    return hit;
}

uint32_t get_near_child_id(uint32_t node_id, node_t *p_nodes) {
    return p_nodes[node_id].first_index;
}

uint32_t get_parent_id(uint32_t node_id, uint32_t *p_parent_ids) {
    return p_parent_ids[node_id];
}

uint32_t get_sibling_id(uint32_t node_id, node_t *p_nodes, uint32_t *p_parent_ids) {
    node_t *parent = &p_nodes[p_parent_ids[node_id]];
    uint32_t left = parent->first_index;
    uint32_t right = parent->first_index + 1;
    return node_id == left ? right : left;
}

hit_t stackless_traverse(bvh_t *bvh, inout ray_t ray, triangle_t *triangles) {
    hit_t hit;

    const uint from_parent = 0;
    const uint from_sibling = 1;
    const uint from_child = 2;

    uint state = from_parent;
    uint32_t current_id = get_near_child_id(0, bvh->p_nodes);

    while (true) {
        hit.loops++;
        if (state == from_child)
        {
            if (current_id == 0) return hit;
            if (current_id == get_near_child_id(get_parent_id(current_id, bvh->p_parent_ids), bvh->p_nodes)) {
                current_id = get_sibling_id(current_id, bvh->p_nodes, bvh->p_parent_ids);
                state = from_sibling;
            } else {
                current_id = get_parent_id(current_id, bvh->p_parent_ids);
                state = from_child;
            }
        }

        if (state == from_sibling)
        {
            node_t current_node = bvh->p_nodes[current_id];
            hit.node_intersect_count++;
            if (!node_intersect(current_node, ray)) {
                current_id = get_parent_id(current_id, bvh->p_parent_ids);
                state = from_child;
            } else if (current_node.is_leaf()) {
                for (uint32_t i = 0; i < current_node.primitive_count; i++) {
                    const uint32_t primitive_id = bvh->p_primitive_indices[current_node.first_index + i];
                    hit.primitive_intersect_count++;
                    triangle_intersection_t intersection = triangle_intersect(triangles[primitive_id], ray);
                    if (intersection.did_intersect()) {
                        ray.tmax = intersection.t;
                        hit.primitive_id = primitive_id;
                    }
                }
                current_id = get_parent_id(current_id, bvh->p_parent_ids);
                state = from_child;
            } else {
                current_id = get_near_child_id(current_id, bvh->p_nodes);
                state = from_parent;
            }
        }

        if (state == from_parent)
        {
            node_t current_node = bvh->p_nodes[current_id];
            hit.node_intersect_count++;
            if (!node_intersect(current_node, ray)) {
                current_id = get_sibling_id(current_id, bvh->p_nodes, bvh->p_parent_ids);
                state = from_sibling;
            } else if (current_node.is_leaf()) {
                for (uint32_t i = 0; i < current_node.primitive_count; i++) {
                    const uint32_t primitive_id = bvh->p_primitive_indices[current_node.first_index + i];
                    hit.primitive_intersect_count++;
                    triangle_intersection_t intersection = triangle_intersect(triangles[primitive_id], ray);
                    if (intersection.did_intersect()) {
                        ray.tmax = intersection.t;
                        hit.primitive_id = primitive_id;
                    }
                }
                current_id = get_sibling_id(current_id, bvh->p_nodes, bvh->p_parent_ids);
                state = from_sibling;
            } else {
                current_id = get_near_child_id(current_id, bvh->p_nodes);
                state = from_parent;
            }
        }
    }
    return hit;
}

float4 operator *(float4x4 mat, float4 vec) {
    return mul(mat, vec);
}

extension float3 {
    __init(float4 v) {
        this.x = v.x;
        this.y = v.y;
        this.z = v.z;
    }
};

ray_t create_ray(in float2 uv, in float4x4 inv_projection, in float4x4 inv_view) {
    float2 px_nds = uv * 2.f - 1.f;
    // float2 px_nds = uv;
    float3 point_nds = float3(px_nds, -1);
    float4 point_ndsh = float4(point_nds, 1);
    float4 dir_eye = inv_projection * point_ndsh;
    dir_eye.w = 0;
    float3 dir_world = float3(inv_view * dir_eye);
    float3 eye = { inv_view[3][0], inv_view[3][1], inv_view[3][2] };
    ray_t ray = ray_t::create(eye, dir_world);
    return ray;
}

[shader("fragment")]
pixel_t fragmentMain(per_fragment_t per_fragment) : SV_Target {
    pixel_t p;

    float3 eye = { 0, 2.5, 0 };
    float3 dir = { -1, 0, 0 };
    float3 up = { 0, 1, 0 };

    // float3 eye = { 0, 2.5, 5 };
    // float3 dir = { 0, 0, -1 };
    // float3 up = { 0, 1, 0 };

    float3 right = normalize(cross(normalize(dir), up));
    up = cross(right, normalize(dir));

    float r = per_fragment.uv.x;
    float g = per_fragment.uv.y;

    float u = -(r * 2.f - 1.f);
    float v = g * 2.f - 1.f;

    float3 ray_dir = dir + (u * right) + (v * up);
    ray_t ray = ray_t::create(eye, ray_dir);
    // ray_t ray = create_ray(per_fragment.uv, uniform.inverse_projection, uniform.inverse_view);

    // hit_t hit = stackless_traverse(uniform.bvh, ray, uniform.triangles);
    hit_t hit = stack_traverse(uniform.bvh, ray, uniform.triangles);

    float4 color = { 0 };
    if (hit.did_intersect()) {
        color = float4(((((hit.primitive_id + 1) * 5678) % 255)) / 255.f, ((((hit.primitive_id + 1) * 543) % 255)) / 255.f, ((((hit.primitive_id + 1) * 567) % 255)) / 255.f, 1);
        // color = float4(hit.t / 10.f, hit.t / 10.f, hit.t / 10.f, 1);
        // color.x += hit.node_intersect_count / 100.f;
        // color.y += hit.primitive_intersect_count / 50.f;
        // color.z += hit.loops / 1.f;
        // color.x = hit.loops / 1.f;
    } else {
        // color.x = hit.loops;
    }

    // if (hit.passed == 1) {
    //     color.x = 1;
    // }
    color.w = 1;
    p.color = color;

    return p;
}
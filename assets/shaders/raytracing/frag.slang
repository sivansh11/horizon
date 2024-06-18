import vert;

struct pixel_t {
    float4 color : COLOR0;
};

struct node_t {
    bool is_leaf() { return primitive_count != 0; }
    float3 min;
    uint32_t first_index;
    float3 max;
    uint32_t primitive_count;
};

struct bvh_t {
    node_t *p_nodes;
    uint32_t *p_primitive_indices;
    uint32_t node_count;
    uint32_t primitive_count;
    uint32_t *p_parent_ids;
};

struct vertex_t {
    float3 position;
    float3 normal;
    float2 uv;
    float3 tangent;
    float3 bi_tangent;
};

struct triangle_t {
    vertex_t from_barry(float u, float v, float w) {
        vertex_t vertex;
        vertex.position   = u * v0.position   + v * v1.position   + w * v2.position;
        vertex.normal     = u * v0.normal     + v * v1.normal     + w * v2.normal;
        vertex.uv         = u * v0.uv         + v * v1.uv         + w * v2.uv;
        vertex.tangent    = u * v0.tangent    + v * v1.tangent    + w * v2.tangent;
        vertex.bi_tangent = u * v0.bi_tangent + v * v1.bi_tangent + w * v2.bi_tangent;
        return vertex;
    }

    vertex_t v0, v1, v2;
};

struct blas_instance_t {
    bvh_t *bvh;
    triangle_t *triangles;
    float4x4 inverse_transform;
};

struct uniform_t {
    bvh_t *bvh;
    triangle_t *triangles;
    // blas_instance_t *blas_instances;
    // uint32_t blas_instace_count;
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

static const float infinity = 10000000;
struct ray_t {
    static ray_t create(float3 origin, float3 direction) {
        ray_t ray;
        ray.origin = origin;
        ray.direction = direction;
        ray.tmin = 0;
        ray.tmax = infinity;
        ray.inverse_direction = { safe_inverse(direction.x), safe_inverse(direction.y), safe_inverse(direction.z) };
        return ray;
    }

    static ray_t create(in float2 uv, in float4x4 inv_projection, in float4x4 inv_view) {
        float2 px_nds = uv * 2.f - 1.f;
        float3 point_nds = float3(px_nds, -1);
        float4 point_ndsh = float4(point_nds, 1);
        float4 dir_eye = inv_projection * point_ndsh;
        dir_eye.w = 0;
        float3 dir_world = float3(inv_view * dir_eye);
        float3 eye = { inv_view[0][3], inv_view[1][3], inv_view[2][3] };
        ray_t ray = ray_t::create(eye, dir_world);
        return ray;
    }

    float3 origin, direction, inverse_direction;
    float tmin, tmax;
};

struct triangle_intersection_t {
    bool did_intersect() { return intersection; }
    bool intersection;
    float t;
    float u, v, w;
};

triangle_intersection_t triangle_intersect(triangle_t triangle, ray_t ray) {
    float3 e1 = triangle.v0.position - triangle.v1.position;
    float3 e2 = triangle.v2.position - triangle.v0.position;
    float3 n = cross(e1, e2);

    float3 c = triangle.v0.position - ray.origin;
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
            intersection.u = u;
            intersection.v = v;
            intersection.w = w;
            return intersection;
        }
    }
    triangle_intersection_t intersection;
    intersection.intersection = false;
    intersection.t = infinity;
    return intersection;
}

struct node_intersection_t {
    bool did_intersect() {
        return tmin <= tmax;
    }
    float tmin, tmax;
}

node_intersection_t node_intersect(node_t node, ray_t ray) {
    float3 tmin = (node.min - ray.origin) * ray.inverse_direction;
    float3 tmax = (node.max - ray.origin) * ray.inverse_direction;

    const float3 old_tmin = tmin;
    const float3 old_tmax = tmax;

    tmin = min(old_tmin, old_tmax);
    tmax = max(old_tmin, old_tmax);

    float _tmin = max(tmin[0], max(tmin[1], max(tmin[2], ray.tmin)));
    float _tmax = min(tmax[0], min(tmax[1], min(tmax[2], ray.tmax)));

    node_intersection_t node_intersection = { _tmin, _tmax };
    return node_intersection;
}

static const uint32_t null_primitive_id = uint32_t(-1);
struct hit_t {
    bool did_intersect() { return primitive_id != null_primitive_id; }
    uint32_t primitive_id = null_primitive_id;
    float t = infinity;
    float u, v, w;
    uint32_t node_intersect_count = 0;
    uint32_t primitive_intersect_count = 0;
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
            hit.u = intersection.u;
            hit.v = intersection.v;
            hit.w = intersection.w;
        }
    }
}

hit_t stack_traverse_ordered(bvh_t *bvh, inout ray_t ray, triangle_t *triangles) {
    hit_t hit;

    uint32_t stack[32];
    uint32_t stack_top = 0;

    stack[stack_top++] = 0;

    while (stack_top != 0) {
        node_t *node = &bvh->p_nodes[stack[--stack_top]];

        if (node.is_leaf()) {
            intersect_leaf_node(node, hit, bvh, ray, triangles);
        } else {
            hit.node_intersect_count += 2;
            node_intersection_t left = node_intersect(bvh->p_nodes[node.first_index], ray);
            node_intersection_t right = node_intersect(bvh->p_nodes[node.first_index + 1], ray);

            if (left.did_intersect()) {
                if (right.did_intersect()) {
                    if (left.tmin <= right.tmin) {
                        stack[stack_top++] = node.first_index + 1;
                        stack[stack_top++] = node.first_index;
                    } else {
                        stack[stack_top++] = node.first_index;
                        stack[stack_top++] = node.first_index + 1;
                    }
                } else {
                    stack[stack_top++] = node.first_index;
                }
            } else {
                if (right.did_intersect()) {
                    stack[stack_top++] = node.first_index + 1;
                }
            }
        }
    }
    return hit;
}

hit_t stack_traverse_ordered_optimised(bvh_t *bvh, inout ray_t ray, triangle_t *triangles) {
    hit_t hit;

    uint32_t stack[32];
    uint32_t stack_top = 0;

    uint32_t current = 0;

    while (true) {
        node_t *node = &bvh->p_nodes[current];

        if (node.is_leaf()) {
            intersect_leaf_node(node, hit, bvh, ray, triangles);
            if (stack_top == 0) return hit;
            current = stack[--stack_top];
        } else {
            hit.node_intersect_count += 2;
            node_intersection_t left = node_intersect(bvh->p_nodes[node.first_index], ray);
            node_intersection_t right = node_intersect(bvh->p_nodes[node.first_index + 1], ray);

            if (left.did_intersect()) {
                if (right.did_intersect()) {
                    if (left.tmin <= right.tmin) {
                        stack[stack_top++] = node.first_index + 1;
                        current = node.first_index;
                    } else {
                        stack[stack_top++] = node.first_index;
                        current = node.first_index + 1;
                    }
                } else {
                    current = node.first_index;
                }
            } else {
                if (right.did_intersect()) {
                    current = node.first_index + 1;
                } else {
                    if (stack_top == 0) return hit;
                    current = stack[--stack_top];
                }
            }
        }
    }
    return hit;
}

hit_t stack_traverse_ordered_optimised_further(bvh_t *bvh, inout ray_t ray, triangle_t *triangles) {
    hit_t hit;
    {
        node_t *root = &bvh->p_nodes[0];
        hit.node_intersect_count++;
        if (!node_intersect(*root, ray).did_intersect()) return hit;
        if (root->is_leaf()) {
            intersect_leaf_node(root, hit, bvh, ray, triangles);
            return hit;
        }
    }

    uint32_t stack[32];
    uint32_t stack_top = 0;

    uint32_t current = 0;

    node_t *left_node = &bvh->p_nodes[1];
    node_t *right_node = &bvh->p_nodes[2];
    hit.node_intersect_count += 2;
    node_intersection_t left = node_intersect(*left_node, ray);
    node_intersection_t right = node_intersect(*right_node, ray);

    if (left.did_intersect()) {
        if (right.did_intersect()) {
            if (left.tmin <= right.tmin) {
                stack[stack_top++] = 2;
                current = 1;
            } else {
                stack[stack_top++] = 1;
                current = 2;
            }
        } else {
            current = 1;
        }
    } else {
        if (right.did_intersect()) {
            current = 2;
        } else {
            if (stack_top == 0) return hit;
            current = stack[--stack_top];
        }
    }

    while (true) {
        node_t *node = &bvh->p_nodes[current];

        hit.node_intersect_count += 2;
        node_t *left_node = &bvh->p_nodes[node.first_index];
        node_t *right_node = &bvh->p_nodes[node.first_index + 1];
        node_intersection_t left = node_intersect(*left_node, ray);
        node_intersection_t right = node_intersect(*right_node, ray);

        uint32_t start = left_node.first_index;
        uint32_t end = start + int(left.did_intersect()) * left_node.primitive_count + int(right.did_intersect()) * right_node.primitive_count;
        
        for (uint32_t i = start; i < end; i++) {
            const uint32_t primitive_id = bvh->p_primitive_indices[i];
            hit.primitive_intersect_count++;
            triangle_intersection_t intersection = triangle_intersect(triangles[primitive_id], ray);
            if (intersection.did_intersect()) {
                ray.tmax = intersection.t;
                hit.primitive_id = primitive_id;
                hit.t = intersection.t;
            }
        }

        bool should_traverse_left = left.did_intersect() && !left_node.is_leaf() ? true : false;
        bool should_traverse_right = right.did_intersect() && !right_node.is_leaf() ? true : false; 
            
        if (should_traverse_left) {
            if (should_traverse_right) {
                if (left.tmin <= right.tmin) {
                    stack[stack_top++] = node.first_index + 1;
                    current = node.first_index;
                } else {
                    stack[stack_top++] = node.first_index;
                    current = node.first_index + 1;
                }
            } else {
                current = node.first_index;
            }
        } else {
            if (should_traverse_right) {
                current = node.first_index + 1;
            } else {
                if (stack_top == 0) return hit;
                current = stack[--stack_top];
            }
        }
        
    }
    return hit;
}

hit_t stack_traverse_unordered(bvh_t *bvh, inout ray_t ray, triangle_t *triangles) {
    hit_t hit;

    uint32_t stack[32];
    uint32_t stack_top = 0;

    stack[stack_top++] = 0;

    while (stack_top != 0) {
        node_t *node = &bvh->p_nodes[stack[--stack_top]];

        hit.node_intersect_count++;
        if (!node_intersect(*node, ray).did_intersect()) continue;

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
            if (!node_intersect(current_node, ray).did_intersect()) {
                current_id = get_parent_id(current_id, bvh->p_parent_ids);
                state = from_child;
            } else if (current_node.is_leaf()) {
                for (uint32_t i = 0; i < current_node.primitive_count; i++) {
                    const uint32_t primitive_id = bvh.p_primitive_indices[current_node.first_index + i];
                    hit.primitive_intersect_count++;
                    triangle_intersection_t intersection = triangle_intersect(triangles[primitive_id], ray);
                    if (intersection.did_intersect()) {
                        ray.tmax = intersection.t;
                        hit.primitive_id = primitive_id;
                        hit.t = intersection.t;
                        hit.u = intersection.u;
                        hit.v = intersection.v;
                        hit.w = intersection.w;
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
            if (!node_intersect(current_node, ray).did_intersect()) {
                current_id = get_sibling_id(current_id, bvh->p_nodes, bvh->p_parent_ids);
                state = from_sibling;
            } else if (current_node.is_leaf()) {
                for (uint32_t i = 0; i < current_node.primitive_count; i++) {
                    const uint32_t primitive_id = bvh.p_primitive_indices[current_node.first_index + i];
                    hit.primitive_intersect_count++;
                    triangle_intersection_t intersection = triangle_intersect(triangles[primitive_id], ray);
                    if (intersection.did_intersect()) {
                        ray.tmax = intersection.t;
                        hit.primitive_id = primitive_id;
                        hit.t = intersection.t;
                        hit.u = intersection.u;
                        hit.v = intersection.v;
                        hit.w = intersection.w;
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

float4 operator *(float4 vec, float4x4 mat) {
    return mul(vec, mat);
}

extension float3 {
    __init(float4 v) {
        this.x = v.x;
        this.y = v.y;
        this.z = v.z;
    }
};

float3 lerp_f3(float3 start, float3 end, float t) {
    return start + (end - start) * t;
}

uint pcg_hash(uint seed) {
    uint state = seed * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// Used to advance the PCG state.
uint rand_pcg(inout uint rng_state) {
    uint state = rng_state;
    rng_state = rng_state * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float uintBitsToFloat(uint v) {
    return float(v & 0x7FFFFFFF) / 2.0f + float(v >> 31) * 2147483648.0f;
}

// Advances the prng state and returns the corresponding random float.
float rand(inout uint state) {
    uint x = rand_pcg(state);
    state = x;
    return float(x) * reinterpret<float>(0x2f800004u);
}

float3 rand_float3(inout uint seed) {
    return { rand(seed), rand(seed), rand(seed) };
}

float3 rand_float3_in_unit_sphere(inout uint seed) {
    while (true) {
        float3 r = rand_float3(seed) * 2.f - 1.f;
        if (length(r) < 1) return r;
    }
}

[shader("fragment")]
pixel_t fragmentMain(per_fragment_t per_fragment) : SV_Target {
    pixel_t p;

    // float3 eye = { 1, .5, 0 };
    // float3 dir = { -1, 0, 0 };
    // float3 up = { 0, 1, 0 };

    // // float3 eye = { 0, 2.5, 5 };
    // // float3 dir = { 0, 0, -1 };
    // // float3 up = { 0, 1, 0 };

    // float3 right = normalize(cross(normalize(dir), up));
    // up = cross(right, normalize(dir));

    float r = per_fragment.uv.x;
    float g = per_fragment.uv.y;

    float u = r;
    float v = 1 - g;

    // float3 ray_dir = dir + (u * right) + (v * up);
    // ray_t ray = ray_t::create(eye, ray_dir);

    ray_t ray = ray_t::create({u, v}, uniform.inverse_projection, uniform.inverse_view);

    hit_t hit = stack_traverse_ordered_optimised(uniform.bvh, ray, uniform.triangles);

    p.color = { 0, 0, 0, 1 };
    if (hit.did_intersect()) {
        // p.color = float4(((((hit.primitive_id + 1) * 5678) % 255)) / 255.f, ((((hit.primitive_id + 1) * 543) % 255)) / 255.f, ((((hit.primitive_id + 1) * 567) % 255)) / 255.f, 1);

        float t = hit.node_intersect_count / 200.f;
        if (t > 1) {
            p.color.xyz = { 1, 1, 1 };
        } else {
            p.color.xyz = lerp_f3( { 0, 1, 0 }, { 1, 0, 0 }, t);
        }
    }

    // ray_t ray = ray_t::create(per_fragment.uv, uniform.inverse_projection, uniform.inverse_view);

    // uint pixel_index = uint(uint(per_fragment.uv.x * 3456) + uint(per_fragment.uv.y * 4567) * 10000000);
    // uint seed = pcg_hash(pixel_index);

    // float3 throughput = { 1 };
    // float3 color = { 0 };
    // for (uint32_t bounce = 0; bounce < 5; bounce++) {
    //     hit_t hit = stack_traverse_ordered_optimised(uniform.bvh, ray, uniform.triangles);
    //     if (hit.did_intersect()) {
    //         vertex_t vertex = uniform.triangles[hit.primitive_id].from_barry(hit.u, hit.v, hit.w);
    //         float3 normal = vertex.normal;
    //         ray = ray.create(ray.origin + hit.t * ray.direction + normal * 0.0001f, normalize(normal + rand_float3_in_unit_sphere(seed)));
    //     } else {
    //         p.color.xyz += { 1, 1, 1 };
    //         break;
    //     }
    // }

    // p.color.xyz = { per_fragment.uv.x, per_fragment.uv.y, 0 };

    return p;
}
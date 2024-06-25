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
        vertex.position = u * v0.position + v * v1.position + w * v2.position;
        vertex.normal = u * v0.normal + v * v1.normal + w * v2.normal;
        vertex.uv = u * v0.uv + v * v1.uv + w * v2.uv;
        vertex.tangent = u * v0.tangent + v * v1.tangent + w * v2.tangent;
        vertex.bi_tangent = u * v0.bi_tangent + v * v1.bi_tangent + w * v2.bi_tangent;
        return vertex;
    }

    vertex_t v0, v1, v2;
    uint32_t material_id = 0;
};

struct blas_instance_t {
    bvh_t *p_bvh;
    triangle_t *p_triangles;
    float3 min, max;
    float4x4 inverse_transform;
};

static const uint32_t material_type_lambertian = 0;
static const uint32_t material_type_diffuse_light = 1;

struct material_t {
    uint32_t material_type;
    float3 albedo;
};

struct uniform_t {
    float4x4         inverse_projection;
    float4x4         inverse_view;
    uint32_t         width; 
    uint32_t         height;
    uint32_t         blas_instance_count;
    uint32_t         samples;
    blas_instance_t *p_blas_instances;
    material_t      *p_materials;
    bvh_t           *p_tlas;
};

const static float infinity = 1000000000;

float safe_inverse(float x) {
    const float epsilon = 0.001;
    if (abs(x) <= epsilon) {
        if (x >= 0) return 1.f / epsilon;
        return -1.f / epsilon;
    }
    return 1.f / x;
}

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

struct ray_t {
    static ray_t create(float3 origin, float3 direction) {
        ray_t ray;
        ray.origin = origin;
        ray.direction = direction;
        ray.tmin = 0.0001;
        ray.tmax = infinity;
        ray.inverse_direction = { safe_inverse(direction.x), safe_inverse(direction.y), safe_inverse(direction.z) };
        return ray;
    }

    static ray_t create(in float2 uv, in float4x4 inv_projection, in float4x4 inv_view) {
        float2 px_nds = uv * 2.f - 1.f;
        float3 point_nds = float3(px_nds, -1);
        float4 point_ndsh = float4(point_nds, 1);
        float4 dir_eye = point_ndsh * inv_projection;
        dir_eye.w = 0;
        float3 dir_world = float3(dir_eye * inv_view);
        float3 eye = { inv_view[3][0], inv_view[3][1], inv_view[3][2] };
        ray_t ray = ray_t::create(eye, dir_world);
        return ray;
    }

    float3 at_max() {
        return origin + tmax * direction;
    } 

    float3 origin, direction, inverse_direction;
    float tmin, tmax;
};

float length2(float3 v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

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

static const uint32_t null_id = uint32_t(-1);

struct hit_t {
    bool did_intersect() { return instance_id != null_id && blas_hit.did_intersect(); }
    uint32_t instance_id = null_id;
    // uint32_t primitive_id = null_id;
    // float t = infinity;
    // float u, v, w;
    // uint32_t node_intersect_count = 0;
    // uint32_t primitive_intersect_count = 0;
    blas_instance_intersection_t blas_hit;
    blas_instance_t instance;
    // uint32_t tlas_intersect_count = 0;
};

struct aabb_intersect_t {
    bool did_intersect() {
        return tmin <= tmax;
    }
    float tmin, tmax;
}

aabb_intersect_t aabb_intersect(float3 _min, float3 _max, ray_t ray) {
    float3 tmin = (_min - ray.origin) * ray.inverse_direction;
    float3 tmax = (_max - ray.origin) * ray.inverse_direction;

    const float3 old_tmin = tmin;
    const float3 old_tmax = tmax;

    tmin = min(old_tmin, old_tmax);
    tmax = max(old_tmin, old_tmax);

    float _tmin = max(tmin[0], max(tmin[1], max(tmin[2], ray.tmin)));
    float _tmax = min(tmax[0], min(tmax[1], min(tmax[2], ray.tmax)));

    aabb_intersect_t aabb_intersect = { _tmin, _tmax };
    return aabb_intersect;
}

aabb_intersect_t node_intersect(node_t node, ray_t ray) {
    return aabb_intersect(node.min, node.max, ray);
}

struct blas_instance_intersection_t {
    bool did_intersect() { return primitive_id != null_id; }
    uint32_t primitive_id = null_id;
    float t = infinity;
    float u, v, w;
    triangle_t triangle;
    // uint32_t node_intersect_count = 0;
    // uint32_t primitive_intersect_count = 0;
};

blas_instance_intersection_t blas_instance_intersect(blas_instance_t blas_instance, ray_t ray, uint groupIndex) {
    blas_instance_intersection_t intersection;
    if (aabb_intersect(blas_instance.min, blas_instance.max, ray).did_intersect()) {
        ray_t backup_ray = ray;
        ray.origin = float3(float4(ray.origin, 1) * blas_instance.inverse_transform);
        ray.direction = float3(float4(ray.direction, 0) * blas_instance.inverse_transform);
        ray.inverse_direction = { safe_inverse(ray.direction.x), safe_inverse(ray.direction.y), safe_inverse(ray.direction.z) };
        intersection = intersect_blas(blas_instance.p_bvh, ray, blas_instance.p_triangles, groupIndex);
        backup_ray.tmax = ray.tmax;
        ray = backup_ray;
    } else {
        // printf("didnt even intersect\n");
    }
    return intersection;
}

static const uint32_t stack_size = 16;
groupshared uint32_t stack[work_group_width * work_group_height][stack_size];

blas_instance_intersection_t intersect_blas(bvh_t *p_blas, ray_t ray, triangle_t *p_triangles, uint groupIndex) {
    blas_instance_intersection_t blas_intersection;

    // uint32_t stack[stack_size];
    uint32_t stack_top = 0;

    node_t root = p_blas->p_nodes[0];
    // blas_intersection.node_intersect_count++;
    if (!node_intersect(root, ray).did_intersect()) return blas_intersection;

    if (root.is_leaf()) {
        for (uint32_t i = 0; i < root.primitive_count; i++) {
            const uint final_id = p_blas.p_primitive_indices[root.first_index + i];
            // blas_intersection.primitive_intersect_count++;
            triangle_t triangle = p_triangles[final_id];
            triangle_intersection_t intersection = triangle_intersect(triangle, ray);
            if (intersection.did_intersect()) {
                ray.tmax = intersection.t;
                blas_intersection.primitive_id = final_id;
                blas_intersection.triangle = triangle;
                blas_intersection.t = intersection.t;
                blas_intersection.u = intersection.u;
                blas_intersection.v = intersection.v;
                blas_intersection.w = intersection.w;
            }
        }
        return blas_intersection;
    }

    uint current = 1;

    uint count = 0;
    while (true) {

        node_t left = p_blas->p_nodes[current];
        node_t right = p_blas->p_nodes[current + 1];

        // blas_intersection.node_intersect_count += 2;
        aabb_intersect_t left_intersection = node_intersect(left, ray);
        aabb_intersect_t right_intersection = node_intersect(right, ray);

        uint32_t start = 0;
        uint32_t end = 0;
        if (left_intersection.did_intersect() && left.is_leaf()) {
            if (right_intersection.did_intersect() && right.is_leaf()) {
                start = left.first_index;
                end = right.first_index + right.primitive_count;
            } else {
                start = left.first_index;
                end = left.first_index + left.primitive_count;
            }
        } else if (right_intersection.did_intersect() && right.is_leaf()) {
            start = right.first_index;
            end = right.first_index + right.primitive_count;
        }
        for (uint32_t primitive_id = start; primitive_id < end; primitive_id++) {
            const uint final_id = p_blas.p_primitive_indices[primitive_id];
            // blas_intersection.primitive_intersect_count++;
            triangle_t triangle = p_triangles[final_id];
            triangle_intersection_t intersection = triangle_intersect(triangle, ray);
            if (intersection.did_intersect()) {
                ray.tmax = intersection.t;
                blas_intersection.triangle = triangle;
                blas_intersection.primitive_id = final_id;
                blas_intersection.t = intersection.t;
                blas_intersection.u = intersection.u;
                blas_intersection.v = intersection.v;
                blas_intersection.w = intersection.w;
            }
        }

        if (left_intersection.did_intersect() && !left.is_leaf()) {
            if (right_intersection.did_intersect() && !right.is_leaf()) {
                if (stack_top >= stack_size) return blas_intersection;
                if (left_intersection.tmin <= right_intersection.tmin) {
                    current = left.first_index;                         // traverse left first
                    stack[groupIndex][stack_top++] = right.first_index; // traverse rigth later
                } else {
                    current = right.first_index;                       // traverse right first
                    stack[groupIndex][stack_top++] = left.first_index; // traverse left later
                }
            } else {
                current = left.first_index;
            }
        } else {
            if (right_intersection.did_intersect() && !right.is_leaf()) {
                current = right.first_index;
            } else {
                if (stack_top == 0) return blas_intersection;
                current = stack[groupIndex][--stack_top];
            }
        }
    }

    return blas_intersection;
}

hit_t stack_traverse_ordered_optimised_tlas(inout ray_t ray, uint groupIndex) {
    hit_t hit;

    uint32_t local_stack[8];
    uint32_t stack_top = 0;

    uint32_t current = 0;
    // hit.tlas_intersect_count++;
    // hit.blas_hit.node_intersect_count++;
    if (!node_intersect(uni.p_tlas.p_nodes[0], ray).did_intersect()) return hit;

    // uint count = 0;
    while (true) {
        // if (count++ > 1000) return hit;
        node_t *node = &uni.p_tlas->p_nodes[current];

        if (node.is_leaf()) {
            for (uint32_t i = 0; i < node.primitive_count; i++) {
                const uint32_t primitive_id = uni.p_tlas.p_primitive_indices[node.first_index + i];
                // hit.blas_hit.primitive_intersect_count++;
                blas_instance_t instance = uni.p_blas_instances[primitive_id];
                blas_instance_intersection_t intersection = blas_instance_intersect(instance, ray, groupIndex);
                if (intersection.did_intersect()) {
                    ray.tmax = intersection.t;
                    hit.instance_id = primitive_id;
                    hit.instance = instance;
                    hit.blas_hit = intersection;
                    // hit.blas_hit.triangle = intersection.triangle;
                    // hit.blas_hit.primitive_id = intersection.primitive_id;
                    // hit.blas_hit.t = intersection.t;
                    // hit.blas_hit.u = intersection.u;
                    // hit.blas_hit.v = intersection.v;
                    // hit.blas_hit.w = intersection.w;
                    // hit.blas_hit.node_intersect_count += intersection.node_intersect_count;
                    // hit.blas_hit.primitive_intersect_count += intersection.primitive_intersect_count;
                }
            }
            if (stack_top == 0) return hit;
            current = local_stack[--stack_top];
        } else {
            // hit.blas_hit.node_intersect_count += 2;
            // hit.tlas_intersect_count++;
            // hit.tlas_intersect_count++;
            aabb_intersect_t left = node_intersect(uni.p_tlas->p_nodes[node.first_index], ray);
            aabb_intersect_t right = node_intersect(uni.p_tlas->p_nodes[node.first_index + 1], ray);

            if (left.did_intersect()) {
                if (right.did_intersect()) {
                    if (stack_top >= stack_size) return hit;
                    if (left.tmin <= right.tmin) {
                        local_stack[stack_top++] = node.first_index + 1;
                        current = node.first_index;
                    } else {
                        local_stack[stack_top++] = node.first_index;
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
                    current = local_stack[--stack_top];
                }
            }
        }
    }
    return hit;
}

hit_t bvh_traverse_no_tlas(ray_t ray, uint groupIndex) {
    hit_t hit;
    for (uint32_t i = 0; i < uni.blas_instance_count; i++) {
        // hit.blas_hit.primitive_intersect_count++;
        // blas_instance_intersection_t intersection = intersect_blas(uni.p_blas_instances[i].p_bvh, ray, uni.p_blas_instances[i].p_triangles, groupIndex);
        blas_instance_t instance = uni.p_blas_instances[i];
        blas_instance_intersection_t intersection = blas_instance_intersect(instance, ray, groupIndex);
        if (intersection.did_intersect()) {
            printf("%i\n", i);
            ray.tmax = intersection.t;
            hit.instance_id = i;
            hit.instance = instance;
            hit.blas_hit = intersection;
            // hit.blas_hit.triangle = intersection.triangle;
            // hit.blas_hit.primitive_id = intersection.primitive_id;
            // hit.blas_hit.t = intersection.t;
            // hit.blas_hit.u = intersection.u;
            // hit.blas_hit.v = intersection.v;
            // hit.blas_hit.w = intersection.w;
            // hit.blas_hit.node_intersect_count += intersection.node_intersect_count;
            // hit.blas_hit.primitive_intersect_count += intersection.primitive_intersect_count;
        }
    }
    printf("-----------\n");
    return hit;
}

// hit_t traverse_no_bvh(ray_t ray, triangle_t *triangles, uint32_t blas_instance_count) {
//     hit_t hit;
//     for (uint32_t i = 0; i < blas_instance_count; i++) {
//         hit.primitive_intersect_count++;
//         triangle_intersection_t intersection = triangle_intersect(triangles[i], ray);
//         if (intersection.did_intersect()) {
//             ray.tmax = intersection.t;
//             hit.primitive_id = i;
//             hit.t = intersection.t;
//             hit.u = intersection.u;
//             hit.v = intersection.v;
//             hit.w = intersection.w;
//         }
//     }
//     return hit;
// }

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

// Advances the prng state and returns the corresponding random float.
float rand(inout uint state) {
    uint x = rand_pcg(state);
    state = x;
    return float(x) * reinterpret<float>(0x2f800004u);
}

float3 rand_float3(inout uint seed) {
    return { rand(seed), rand(seed), rand(seed) };
}

float2 rand_float2(inout uint seed) {
    return { rand(seed), rand(seed) };
}

// probably should be hidden
float3 rand_float3_in_unit_sphere(inout uint seed) {
    uint32_t count = 0;
    while (true) {
        count++;
        if (count > 100) return { 0, 0, 1 };
        float3 r = rand_float3(seed) * 2.f - 1.f;
        if (length(r) < 1) return r;
    }
}

float3 rand_float3_unit_sphere(inout uint seed) {
    return normalize(rand_float3_in_unit_sphere(seed));
}

float3 rand_float3_hemisphere(inout uint seed, float3 normal) {
    float3 unit_sphere = rand_float3_unit_sphere(seed);
    if (dot(unit_sphere, normal) > 0.0) return unit_sphere;
    return -unit_sphere;
}

bool near_zero(float3 v) {
    const float s = 1e-8;
    return (abs(v.x) < s) && (abs(v.y) < s) && (abs(v.z) < s);
}

float3 lerp_f3(float3 start, float3 end, float t) {
    return start + (end - start) * t;
}

float3 refract(float3 dir, float3 n, float ri) {
    const float cos_theta = min(dot(-dir, n), 1.0);
    float3 r_out_perp = ri * (dir + cos_theta * n);
    float3 r_out_para = -sqrt(abs(1.0 - length2(r_out_perp))) * n;
    return r_out_perp + r_out_para;
}

float reflectance(float cos, float ri) {
    float r0 = (1 - ri) / (1 + ri);
    r0 = r0 * r0;
    return r0 + (1 - r0) * pow((1 - cos), 5);
}

uniform uniform_t uni;
uniform RWTexture2D<float4> o_color;

float3 background(ray_t ray) {
    // float3 unit_direction = normalize(ray.direction);
    // float a = 0.5 * (unit_direction.y + 1.0);
    // return (1.0 - a) * float3(1, 1, 1) + a * float3(0.3, 0.4, .7);
    // return { 0, 0, 0 };
    return { 1, 1, 1 };
}

bool material_scatter(in material_t material, inout uint seed, in ray_t r_in, hit_t hit, out float3 attenuation, out ray_t scattered) {
    // sphere_t sphere = uni.p_spheres[hit.primitive_id];
    // float3 N = normalize((r_in.origin + hit.t * r_in.direction) - sphere.center);
    // bool front_face = dot(r_in.direction, N) < 0;
    // N = front_face ? N : -N;

    vertex_t vertex = hit.blas_hit.triangle.from_barry(hit.blas_hit.u, hit.blas_hit.v, hit.blas_hit.w);
    float3 N = vertex.normal;
    if (material.material_type == material_type_lambertian) {
        float3 scatter_direction = N + rand_float3_unit_sphere(seed);

        if (near_zero(scatter_direction))
            scatter_direction = N;

        scattered = ray_t.create(r_in.origin + hit.blas_hit.t * r_in.direction, scatter_direction);
        attenuation = material.albedo;
        return true;
    }
    if (material.material_type == material_type_diffuse_light) {
        attenuation = {};
        scattered = {};
        return false;
    }
    scattered = ray_t();
    attenuation = {};
    return false;
}

float3 material_emitted(in material_t material, hit_t hit) {
    if (material.material_type == material_type_lambertian) {
        return { 0, 0, 0 };
    }
    if (material.material_type == material_type_diffuse_light) {
        return material.albedo;
    }
    // if (material.material_type == material_type_metal) {
    //     return { 0, 0, 0 };
    // }
    // if (material.material_type == material_type_dielectric) {
    //     return { 0, 0, 0 };
    // }
    return { 0, 0, 0 };
}

// float3 ray_color(ray_t ray, inout uint seed, sphere_t *p_spheres, uint32_t sphere_count) {
//     const uint32_t bounce_count = 5;

//     float3 color = { 0, 0, 0 };
//     float3 throughput = { 1, 1, 1 };

//     for (uint32_t bounce = 0; bounce < bounce_count; bounce++) {
//         hit_t hit = traverse_no_bvh(ray, p_spheres, sphere_count);
//         if (hit.did_intersect()) {
//             material_t material = p_materials[p_spheres[hit.primitive_id].material_id];
//             float3 attenuation;
//             float3 emmision = { 0, 0, 0 };
//             ray_t scattered;
//             if (material_scatter(material, seed, ray, hit, attenuation, scattered)) {
//                 throughput = (attenuation * throughput) + emmision;
//                 ray = scattered;
//             } else {
//                 throughput *= emmision;
//                 color = throughput;
//                 break;
//             }


//         } else {
//             color = throughput * background(ray);
//             break;
//         }
//     }
//     return color;
// }

bool russian_roulette_terminate_ray(inout float3 throughput, inout uint seed) {
    float p = max(throughput.x, max(throughput.y, throughput.z));
    if (rand(seed) > p) {
        return true;
    }
    throughput /= p;
    return false;
}

float3 ray_color(ray_t ray, inout uint seed, uint groupIndex) {
    const uint32_t bounce_count = 5;

    float3 color = { 0, 0, 0 };
    float3 throughput = { 1, 1, 1 };

    for (uint32_t bounce = 0; bounce < bounce_count; bounce++) {
        // hit_t hit = stack_traverse_ordered_optimised_further(uni.p_bvh, ray, p_triangles, groupIndex);
        // hit_t hit = stack_traverse_ordered_optimised(uni.p_bvh, ray, p_triangles, groupIndex);
        // hit_t hit = traverse_no_bvh(ray, p_triangles, blas_instance_count);

        hit_t hit = stack_traverse_ordered_optimised_tlas(ray, groupIndex);

        if (!hit.did_intersect()) {
            color += throughput * background(ray);
            break;
        }

        // material_t material = uni.p_materials[p_triangles[hit.primitive_id].material_id];
        material_t material = uni.p_materials[hit.blas_hit.triangle.material_id];
        float3 emission = material_emitted(material, hit);
        color += throughput * emission;
        float3 attenuation;
        ray_t scattered;

        if (material_scatter(material, seed, ray, hit, attenuation, scattered)) {
            throughput = throughput * attenuation;
            ray = scattered;
        } else {
            break;
        }

        if (russian_roulette_terminate_ray(throughput, seed)) {
            break;
        }
    }

    return color;
}

static const uint32_t work_group_width = 8;
static const uint32_t work_group_height = 8;
[shader("compute")]
[numthreads(work_group_width, work_group_height, 1)]
void computeMain(uint3 dispatchThreadID: SV_DispatchThreadID, uint groupIndex: SV_GroupIndex) {

    if (dispatchThreadID.x >= uni.width) return;
    if (dispatchThreadID.y >= uni.height) return;

    float4 color = { 0, 0, 0, 1 };

    const float u = float(dispatchThreadID.x) / float(uni.width - 1);
    const float v = 1 - float(dispatchThreadID.y) / float(uni.height - 1);

    uint seed = uint(dispatchThreadID.x + uni.width * (dispatchThreadID.y + uni.height * uni.samples));

    float2 jitter = (rand_float2(seed) * 2.f - 1.f) / 1000.f;
    ray_t ray = ray_t::create(float2(u, v) + jitter, uni.inverse_projection, uni.inverse_view);
    float3 new_color = ray_color(ray, seed, groupIndex);
    float3 old_color = o_color[uint2(dispatchThreadID.x, dispatchThreadID.y)].xyz;
    color.xyz = (old_color * (uni.samples - 1) + new_color) / float(uni.samples);

    // ray_t ray = ray_t::create( { u, v }, uni.inverse_projection, uni.inverse_view);
    // hit_t hit = stack_traverse_ordered_optimised_tlas(ray, groupIndex);
    // // hit_t hit = bvh_traverse_no_tlas(ray, groupIndex);
    // if (hit.did_intersect()) {
    //     color.xyz = float3(((((hit.instance_id + 1) * 5678) % 255)) / 255.f, ((((hit.instance_id + 1) * 543) % 255)) / 255.f, ((((hit.instance_id + 1) * 567) % 255)) / 255.f);

    //     // float t = hit.node_intersect_count / 250.f;
    //     // if (t > 1) {
    //     //     color.xyz = { 1, 1, 1 };
    //     // } else {
    //     //     color.xyz = lerp_f3( { 0, 1, 0 }, { 1, 0, 0 }, t);
    //     // }
    // }
    

    o_color[uint2(dispatchThreadID.x, dispatchThreadID.y)] = color;
}
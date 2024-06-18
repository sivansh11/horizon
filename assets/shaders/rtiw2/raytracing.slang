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

struct sphere_t {
    float3 center;
    float r;
    uint32_t material_id;
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
    uint32_t material_id;
};

static const uint32_t material_type_lambertian = 0;
static const uint32_t material_type_diffuse_light = 1;

struct material_t {
    uint32_t material_type;
    float3 albedo;
};

struct uniform_t {
    float4x4    inverse_projection;
    float4x4    inverse_view;
    uint32_t    width; 
    uint32_t    height;
    uint32_t    triangle_count;
    uint32_t    samples;
    triangle_t *p_triangles;
    material_t *p_materials;
    bvh_t      *p_bvh;
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

struct sphere_intersection_t {
    bool did_intersect() { return intersection; }
    bool intersection = false;
    float t = -1;
};

sphere_intersection_t sphere_intersect(sphere_t sphere, ray_t ray) {
    sphere_intersection_t intersection;
    const float3 oc = sphere.center - ray.origin;
    const float a = length2(ray.direction);
    const float h = dot(ray.direction, oc);
    const float c = length2(oc) - sphere.r * sphere.r;
    const float discriminant = h * h - a * c;

    if (discriminant < 0) return intersection;

    const float sqrtd = sqrt(discriminant);
    if (sqrtd < 0) return intersection;

    float root = (h - sqrtd) / a;
    if (root <= ray.tmin || root >= ray.tmax) {
        root = (h + sqrtd) / a;
        if (root <= ray.tmin || root >= ray.tmax) return intersection;
    }

    intersection.intersection = true;
    intersection.t = root;
    return intersection;
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

static const uint32_t null_primitive_id = uint32_t(-1);

struct hit_t {
    bool did_intersect() { return primitive_id != null_primitive_id; }
    uint32_t primitive_id = null_primitive_id;
    float t = infinity;
    float u, v, w;
    uint32_t node_intersect_count = 0;
    uint32_t primitive_intersect_count = 0;
};

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

void intersect_leaf_node(in const node_t node, inout hit_t hit, bvh_t *bvh, inout ray_t ray, sphere_t *spheres) {
    for (uint32_t i = 0; i < node.primitive_count; i++) {
        const uint32_t primitive_id = bvh.p_primitive_indices[node.first_index + i];
        hit.primitive_intersect_count++;
        sphere_intersection_t intersection = sphere_intersect(spheres[primitive_id], ray);
        if (intersection.did_intersect()) {
            ray.tmax = intersection.t;
            hit.primitive_id = primitive_id;
            hit.t = intersection.t;
        }
    }
}

void intersect_leaf_node(in const node_t node, inout hit_t hit, bvh_t *bvh, inout ray_t ray, triangle_t *triangles) {
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

static const uint32_t stack_size = 16;
groupshared uint32_t stack[work_group_width * work_group_height][16];

hit_t stack_traverse_ordered_optimised(bvh_t *bvh, inout ray_t ray, triangle_t *triangles, uint groupIndex) {
    hit_t hit;
    
    // uint32_t stack[stack_size];
    uint32_t stack_top = 0;

    uint32_t current = 0;

    uint count = 0;
    while (true) {
        if (count++ > 1000) return hit;
        node_t *node = &bvh->p_nodes[current];

        if (node.is_leaf()) {
            intersect_leaf_node(*node, hit, bvh, ray, triangles);
            if (stack_top == 0) return hit;
            current = stack[groupIndex][--stack_top];
        } else {
            hit.node_intersect_count += 2;
            node_intersection_t left = node_intersect(bvh->p_nodes[node.first_index], ray);
            node_intersection_t right = node_intersect(bvh->p_nodes[node.first_index + 1], ray);

            if (left.did_intersect()) {
                if (right.did_intersect()) {
                    if (stack_top >= stack_size) return hit;
                    if (left.tmin <= right.tmin) {
                        stack[groupIndex][stack_top++] = node.first_index + 1;
                        current = node.first_index;
                    } else {
                        stack[groupIndex][stack_top++] = node.first_index;
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
                    current = stack[groupIndex][--stack_top];
                }
            }
        }
    }
    return hit;
}

hit_t stack_traverse_ordered_optimised_further(bvh_t *bvh, inout ray_t ray, triangle_t *triangles, uint groupIndex) {
    hit_t hit;
    // uint32_t stack[stack_size];
    uint32_t stack_top = 0;

    node_t root = bvh->p_nodes[0];
    if (!node_intersect(root, ray).did_intersect()) return hit;

    if (root.is_leaf()) {
        intersect_leaf_node(root, hit, bvh, ray, triangles);
        return hit;
    }

    uint current = 1;

    uint count = 0;
    while (true) {

        node_t left = bvh->p_nodes[current];
        node_t right = bvh->p_nodes[current + 1];

        hit.node_intersect_count += 2;
        node_intersection_t left_intersection = node_intersect(left, ray);
        node_intersection_t right_intersection = node_intersect(right, ray);

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
            const uint final_id = bvh.p_primitive_indices[primitive_id];
            hit.primitive_intersect_count++;
            triangle_intersection_t intersection = triangle_intersect(triangles[final_id], ray);
            if (intersection.did_intersect()) {
                ray.tmax = intersection.t;
                hit.primitive_id = final_id;
                hit.t = intersection.t;
                hit.u = intersection.u;
                hit.v = intersection.v;
                hit.w = intersection.w;
            }
        }

        if (left_intersection.did_intersect() && !left.is_leaf()) {
            if (right_intersection.did_intersect() && !right.is_leaf()) {
                if (stack_top >= stack_size) return hit;
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
                if (stack_top == 0) return hit;
                current = stack[groupIndex][--stack_top];
            }
        }
    }
    return hit;
}

hit_t traverse_no_bvh(ray_t ray, sphere_t *spheres, uint32_t sphere_count) {
    hit_t hit;
    for (uint32_t i = 0; i < sphere_count; i++) {
        hit.primitive_intersect_count++;
        sphere_intersection_t intersection = sphere_intersect(spheres[i], ray);
        if (intersection.did_intersect()) {
            ray.tmax = intersection.t;
            hit.primitive_id = i;
            hit.t = intersection.t;
        }
    }
    return hit;
}

hit_t traverse_no_bvh(ray_t ray, triangle_t *triangles, uint32_t triangle_count) {
    hit_t hit;
    for (uint32_t i = 0; i < triangle_count; i++) {
        hit.primitive_intersect_count++;
        triangle_intersection_t intersection = triangle_intersect(triangles[i], ray);
        if (intersection.did_intersect()) {
            ray.tmax = intersection.t;
            hit.primitive_id = i;
            hit.t = intersection.t;
            hit.u = intersection.u;
            hit.v = intersection.v;
            hit.w = intersection.w;
        }
    }
    return hit;
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
    triangle_t triangle = uni.p_triangles[hit.primitive_id];
    vertex_t vertex = triangle.from_barry(hit.u, hit.v, hit.w);
    float3 N = vertex.normal;
    if (material.material_type == material_type_lambertian) {
        float3 scatter_direction = N + rand_float3_unit_sphere(seed);

        if (near_zero(scatter_direction))
            scatter_direction = N;

        scattered = ray_t.create(r_in.origin + hit.t * r_in.direction, scatter_direction);
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

float3 ray_color(ray_t ray, inout uint seed, triangle_t *p_triangles, uint32_t triangle_count, uint groupIndex) {
    const uint32_t bounce_count = 5;

    float3 color = { 0, 0, 0 };
    float3 throughput = { 1, 1, 1 };

    for (uint32_t bounce = 0; bounce < bounce_count; bounce++) {
        hit_t hit = stack_traverse_ordered_optimised_further(uni.p_bvh, ray, p_triangles, groupIndex);
        // hit_t hit = stack_traverse_ordered_optimised(uni.p_bvh, ray, p_triangles, groupIndex);
        // hit_t hit = traverse_no_bvh(ray, p_triangles, triangle_count);

        if (!hit.did_intersect()) {
            color += throughput * background(ray);
            break;
        }

        material_t material = uni.p_materials[p_triangles[hit.primitive_id].material_id];
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

#ifndef __MATRIX_INCLUDED__
#define __MATRIX_INCLUDED__

#define IDENTITY_MATRIX float4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)

float4x4 inverse(float4x4 m) {
    float n11 = m[0][0], n12 = m[1][0], n13 = m[2][0], n14 = m[3][0];
    float n21 = m[0][1], n22 = m[1][1], n23 = m[2][1], n24 = m[3][1];
    float n31 = m[0][2], n32 = m[1][2], n33 = m[2][2], n34 = m[3][2];
    float n41 = m[0][3], n42 = m[1][3], n43 = m[2][3], n44 = m[3][3];

    float t11 = n23 * n34 * n42 - n24 * n33 * n42 + n24 * n32 * n43 - n22 * n34 * n43 - n23 * n32 * n44 + n22 * n33 * n44;
    float t12 = n14 * n33 * n42 - n13 * n34 * n42 - n14 * n32 * n43 + n12 * n34 * n43 + n13 * n32 * n44 - n12 * n33 * n44;
    float t13 = n13 * n24 * n42 - n14 * n23 * n42 + n14 * n22 * n43 - n12 * n24 * n43 - n13 * n22 * n44 + n12 * n23 * n44;
    float t14 = n14 * n23 * n32 - n13 * n24 * n32 - n14 * n22 * n33 + n12 * n24 * n33 + n13 * n22 * n34 - n12 * n23 * n34;

    float det = n11 * t11 + n21 * t12 + n31 * t13 + n41 * t14;
    float idet = 1.0f / det;

    float4x4 ret;

    ret[0][0] = t11 * idet;
    ret[0][1] = (n24 * n33 * n41 - n23 * n34 * n41 - n24 * n31 * n43 + n21 * n34 * n43 + n23 * n31 * n44 - n21 * n33 * n44) * idet;
    ret[0][2] = (n22 * n34 * n41 - n24 * n32 * n41 + n24 * n31 * n42 - n21 * n34 * n42 - n22 * n31 * n44 + n21 * n32 * n44) * idet;
    ret[0][3] = (n23 * n32 * n41 - n22 * n33 * n41 - n23 * n31 * n42 + n21 * n33 * n42 + n22 * n31 * n43 - n21 * n32 * n43) * idet;

    ret[1][0] = t12 * idet;
    ret[1][1] = (n13 * n34 * n41 - n14 * n33 * n41 + n14 * n31 * n43 - n11 * n34 * n43 - n13 * n31 * n44 + n11 * n33 * n44) * idet;
    ret[1][2] = (n14 * n32 * n41 - n12 * n34 * n41 - n14 * n31 * n42 + n11 * n34 * n42 + n12 * n31 * n44 - n11 * n32 * n44) * idet;
    ret[1][3] = (n12 * n33 * n41 - n13 * n32 * n41 + n13 * n31 * n42 - n11 * n33 * n42 - n12 * n31 * n43 + n11 * n32 * n43) * idet;

    ret[2][0] = t13 * idet;
    ret[2][1] = (n14 * n23 * n41 - n13 * n24 * n41 - n14 * n21 * n43 + n11 * n24 * n43 + n13 * n21 * n44 - n11 * n23 * n44) * idet;
    ret[2][2] = (n12 * n24 * n41 - n14 * n22 * n41 + n14 * n21 * n42 - n11 * n24 * n42 - n12 * n21 * n44 + n11 * n22 * n44) * idet;
    ret[2][3] = (n13 * n22 * n41 - n12 * n23 * n41 - n13 * n21 * n42 + n11 * n23 * n42 + n12 * n21 * n43 - n11 * n22 * n43) * idet;

    ret[3][0] = t14 * idet;
    ret[3][1] = (n13 * n24 * n31 - n14 * n23 * n31 + n14 * n21 * n33 - n11 * n24 * n33 - n13 * n21 * n34 + n11 * n23 * n34) * idet;
    ret[3][2] = (n14 * n22 * n31 - n12 * n24 * n31 - n14 * n21 * n32 + n11 * n24 * n32 + n12 * n21 * n34 - n11 * n22 * n34) * idet;
    ret[3][3] = (n12 * n23 * n31 - n13 * n22 * n31 + n13 * n21 * n32 - n11 * n23 * n32 - n12 * n21 * n33 + n11 * n22 * n33) * idet;

    return ret;
}

// http://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/
float4 matrix_to_quaternion(float4x4 m)
{
    float tr = m[0][0] + m[1][1] + m[2][2];
    float4 q = float4(0, 0, 0, 0);

    if (tr > 0)
    {
        float s = sqrt(tr + 1.0) * 2; // S=4*qw
        q.w = 0.25 * s;
        q.x = (m[2][1] - m[1][2]) / s;
        q.y = (m[0][2] - m[2][0]) / s;
        q.z = (m[1][0] - m[0][1]) / s;
    }
    else if ((m[0][0] > m[1][1]) && (m[0][0] > m[2][2]))
    {
        float s = sqrt(1.0 + m[0][0] - m[1][1] - m[2][2]) * 2; // S=4*qx
        q.w = (m[2][1] - m[1][2]) / s;
        q.x = 0.25 * s;
        q.y = (m[0][1] + m[1][0]) / s;
        q.z = (m[0][2] + m[2][0]) / s;
    }
    else if (m[1][1] > m[2][2])
    {
        float s = sqrt(1.0 + m[1][1] - m[0][0] - m[2][2]) * 2; // S=4*qy
        q.w = (m[0][2] - m[2][0]) / s;
        q.x = (m[0][1] + m[1][0]) / s;
        q.y = 0.25 * s;
        q.z = (m[1][2] + m[2][1]) / s;
    }
    else
    {
        float s = sqrt(1.0 + m[2][2] - m[0][0] - m[1][1]) * 2; // S=4*qz
        q.w = (m[1][0] - m[0][1]) / s;
        q.x = (m[0][2] + m[2][0]) / s;
        q.y = (m[1][2] + m[2][1]) / s;
        q.z = 0.25 * s;
    }

    return q;
}

float4x4 m_scale(float4x4 m, float3 v)
{
    float x = v.x, y = v.y, z = v.z;

    m[0][0] *= x; m[1][0] *= y; m[2][0] *= z;
    m[0][1] *= x; m[1][1] *= y; m[2][1] *= z;
    m[0][2] *= x; m[1][2] *= y; m[2][2] *= z;
    m[0][3] *= x; m[1][3] *= y; m[2][3] *= z;

    return m;
}

float4x4 quaternion_to_matrix(float4 quat)
{
    float4x4 m = float4x4(float4(0, 0, 0, 0), float4(0, 0, 0, 0), float4(0, 0, 0, 0), float4(0, 0, 0, 0));

    float x = quat.x, y = quat.y, z = quat.z, w = quat.w;
    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, xy = x * y2, xz = x * z2;
    float yy = y * y2, yz = y * z2, zz = z * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;

    m[0][0] = 1.0 - (yy + zz);
    m[0][1] = xy - wz;
    m[0][2] = xz + wy;

    m[1][0] = xy + wz;
    m[1][1] = 1.0 - (xx + zz);
    m[1][2] = yz - wx;

    m[2][0] = xz - wy;
    m[2][1] = yz + wx;
    m[2][2] = 1.0 - (xx + yy);

    m[3][3] = 1.0;

    return m;
}

float4x4 m_translate(float4x4 m, float3 v)
{
    float x = v.x, y = v.y, z = v.z;
    m[0][3] = x;
    m[1][3] = y;
    m[2][3] = z;
    return m;
}

float4x4 compose(float3 position, float4 quat, float3 scale)
{
    float4x4 m = quaternion_to_matrix(quat);
    m = m_scale(m, scale);
    m = m_translate(m, position);
    return m;
}

void decompose(in float4x4 m, out float3 position, out float4 rotation, out float3 scale)
{
    float sx = length(float3(m[0][0], m[0][1], m[0][2]));
    float sy = length(float3(m[1][0], m[1][1], m[1][2]));
    float sz = length(float3(m[2][0], m[2][1], m[2][2]));

    // if determine is negative, we need to invert one scale
    float det = determinant(m);
    if (det < 0) {
        sx = -sx;
    }

    position.x = m[3][0];
    position.y = m[3][1];
    position.z = m[3][2];

    // scale the rotation part

    float invSX = 1.0 / sx;
    float invSY = 1.0 / sy;
    float invSZ = 1.0 / sz;

    m[0][0] *= invSX;
    m[0][1] *= invSX;
    m[0][2] *= invSX;

    m[1][0] *= invSY;
    m[1][1] *= invSY;
    m[1][2] *= invSY;

    m[2][0] *= invSZ;
    m[2][1] *= invSZ;
    m[2][2] *= invSZ;

    rotation = matrix_to_quaternion(m);

    scale.x = sx;
    scale.y = sy;
    scale.z = sz;
}

float4x4 axis_matrix(float3 right, float3 up, float3 forward)
{
    float3 xaxis = right;
    float3 yaxis = up;
    float3 zaxis = forward;
    return float4x4(
        xaxis.x, yaxis.x, zaxis.x, 0,
        xaxis.y, yaxis.y, zaxis.y, 0,
        xaxis.z, yaxis.z, zaxis.z, 0,
        0, 0, 0, 1
	);
}

// http://stackoverflow.com/questions/349050/calculating-a-lookat-matrix
float4x4 look_at_matrix(float3 forward, float3 up)
{
    float3 xaxis = normalize(cross(forward, up));
    float3 yaxis = up;
    float3 zaxis = forward;
    return axis_matrix(xaxis, yaxis, zaxis);
}

float4x4 look_at_matrix(float3 at, float3 eye, float3 up)
{
    float3 zaxis = normalize(at - eye);
    float3 xaxis = normalize(cross(up, zaxis));
    float3 yaxis = cross(zaxis, xaxis);
    return axis_matrix(xaxis, yaxis, zaxis);
}

float4x4 extract_rotation_matrix(float4x4 m)
{
    float sx = length(float3(m[0][0], m[0][1], m[0][2]));
    float sy = length(float3(m[1][0], m[1][1], m[1][2]));
    float sz = length(float3(m[2][0], m[2][1], m[2][2]));

    // if determine is negative, we need to invert one scale
    float det = determinant(m);
    if (det < 0) {
        sx = -sx;
    }

    float invSX = 1.0 / sx;
    float invSY = 1.0 / sy;
    float invSZ = 1.0 / sz;

    m[0][0] *= invSX;
    m[0][1] *= invSX;
    m[0][2] *= invSX;
    m[0][3] = 0;

    m[1][0] *= invSY;
    m[1][1] *= invSY;
    m[1][2] *= invSY;
    m[1][3] = 0;

    m[2][0] *= invSZ;
    m[2][1] *= invSZ;
    m[2][2] *= invSZ;
    m[2][3] = 0;

    m[3][0] = 0;
    m[3][1] = 0;
    m[3][2] = 0;
    m[3][3] = 1;

    return m;
}

#endif // __MATRIX_INCLUDED__

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
    float3 new_color = ray_color(ray, seed, uni.p_triangles, uni.triangle_count, groupIndex);
    float3 old_color = o_color[uint2(dispatchThreadID.x, dispatchThreadID.y)].xyz;
    color.xyz = (old_color * (uni.samples - 1) + new_color) / float(uni.samples);

    // ray_t ray = ray_t::create( { u, v }, uni.inverse_projection, uni.inverse_view);
    // // float4x4 model = m_scale(IDENTITY_MATRIX, { 100, 100, 100 });
    // // float4x4 inv_model = inverse(model);
    // // ray_t backup_ray = ray;
    // // ray.origin = float3(float4(ray.origin, 1) * inv_model);
    // // ray.direction = float3(float4(ray.direction, 1) * inv_model);
    // // ray.inverse_direction = { safe_inverse(ray.direction.x), safe_inverse(ray.direction.y), safe_inverse(ray.direction.z) };
    // hit_t hit = stack_traverse_ordered_optimised_further(uni.p_bvh, ray, uni.p_triangles, groupIndex);
    // if (hit.did_intersect()) {
    //     // p.color = float4(((((hit.primitive_id + 1) * 5678) % 255)) / 255.f, ((((hit.primitive_id + 1) * 543) % 255)) / 255.f, ((((hit.primitive_id + 1) * 567) % 255)) / 255.f, 1);

    //     float t = hit.node_intersect_count / 200.f;
    //     if (t > 1) {
    //         color.xyz = { 1, 1, 1 };
    //     } else {
    //         color.xyz = lerp_f3( { 0, 1, 0 }, { 1, 0, 0 }, t);
    //     }
    // }

    o_color[uint2(dispatchThreadID.x, dispatchThreadID.y)] = color;
}
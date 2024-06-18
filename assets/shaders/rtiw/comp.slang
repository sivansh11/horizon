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
static const uint32_t material_type_metal = 1;
static const uint32_t material_type_dielectric = 2;
static const uint32_t material_type_diffuse_light = 3;

struct material_t {
    uint32_t material_type;
    float3 albedo;
    float ri_or_fuzz;
};

struct uniform_t {
    float4x4 inverse_projection;
    float4x4 inverse_view;
    uint32_t samples;
    uint32_t width, height;
    uint32_t sphere_count;
    bvh_t *bvh;
    sphere_t *spheres;
    material_t *materials;
};
uniform uniform_t uni;
uniform RWTexture2D<float4> o_color;

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

static const uint32_t null_primitive_id = uint32_t(-1);

void intersect_leaf_node(in const node_t *node, inout hit_t hit, bvh_t *bvh, inout ray_t ray, sphere_t *spheres) {
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

struct hit_t {
    bool did_intersect() { return primitive_id != null_primitive_id; }
    uint32_t primitive_id = null_primitive_id;
    float t = infinity;
    uint32_t node_intersect_count = 0;
    uint32_t primitive_intersect_count = 0;
};

hit_t stack_traverse_ordered_optimised(bvh_t *bvh, inout ray_t ray, sphere_t *spheres) {
    hit_t hit;

    uint32_t stack[32];
    uint32_t stack_top = 0;

    uint32_t current = 0;

    while (true) {
        node_t *node = &bvh->p_nodes[current];

        if (node.is_leaf()) {
            intersect_leaf_node(node, hit, bvh, ray, spheres);
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
    while (true) {
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

float3 background(ray_t ray) {
    // float3 unit_direction = normalize(ray.direction);
    // float a = 0.5 * (unit_direction.y + 1.0);
    // return (1.0 - a) * float3(1, 1, 1) + a * float3(0.3, 0.4, .7);
    return { 0, 0, 0 };
}

hit_t traverse_no_bvh(ray_t ray, sphere_t *spheres) {
    hit_t hit;
    for (uint32_t i = 0; i < uni.sphere_count; i++) {
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

float3 ray_color(ray_t ray, inout uint seed) {
    float3 color = { 0 };

    uint32_t bounce_count = 15;

    float3 throughput = { 1, 1, 1 };

    for (uint32_t bounce = 0; bounce < bounce_count + 1; bounce++) {
        // hit_t hit = stack_traverse_ordered_optimised(uni.bvh, ray, uni.spheres);
        hit_t hit = traverse_no_bvh(ray, uni.spheres);
        if (hit.did_intersect()) {
            float3 attenuation;
            ray_t scattered;
            material_t material = uni.materials[uni.spheres[hit.primitive_id].material_id];
            float3 emission = material_get_emitted(material, hit);

            if (material_get_scatter(material, seed, ray, hit, attenuation, scattered)) {
                // float scattering_pdf = material.get_scattering_pdf(ray, hit, scattered);
                // float pdf = 1 / (2 * 3.14);
                // float pdf = scattering_pdf;

                // throughput = (attenuation * throughput * scattering_pdf) / pdf + emission;
                throughput = (attenuation * throughput) + emission;
                ray = scattered;
            } else {
                throughput *= emission;
                color += throughput;
                break;
            }
        } else {
            color += throughput * background(ray);
            break;
        }
    }

    return color;
}

bool near_zero(float3 v) {
    const float s = 1e-8;
    return (abs(v.x) < s) && (abs(v.y) < s) && (abs(v.z) < s);
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

bool material_get_scatter(in material_t material, inout uint seed, in ray_t r_in, hit_t hit, out float3 attenuation, out ray_t scattered) {
    sphere_t sphere = uni.spheres[hit.primitive_id];
    float3 N = normalize((r_in.origin + hit.t * r_in.direction) - sphere.center);
    bool front_face = dot(r_in.direction, N) < 0;
    N = front_face ? N : -N;
    if (material.material_type == material_type_lambertian) {
        float3 scatter_direction = N + rand_float3_unit_sphere(seed);

        if (near_zero(scatter_direction))
            scatter_direction = N;

        scattered = ray_t.create(r_in.origin + hit.t * r_in.direction, scatter_direction);
        attenuation = material.albedo;
        return true;
    }
    if (material.material_type == material_type_metal) {
        float3 reflected = normalize(reflect(r_in.direction, N)) + (material.ri_or_fuzz * rand_float3_unit_sphere(seed));
        scattered = ray_t.create(r_in.origin + hit.t * r_in.direction, reflected);
        attenuation = material.albedo;
        return (dot(scattered.direction, N) > 0);
    }
    if (material.material_type == material_type_dielectric) {
        float ri = front_face ? 1.0 / material.ri_or_fuzz : material.ri_or_fuzz;
        float3 normalized_dir = normalize(r_in.direction);
        float cos = min(dot(-normalized_dir, N), 1);
        float sin = sqrt(1.0 - cos * cos);
        bool cannot_refract = ri * sin > 1;

        float3 final_dir;

        if (cannot_refract || reflectance(cos, ri) > rand(seed)) {
            final_dir = reflect(normalized_dir, N);
        } else {
            final_dir = refract(normalized_dir, N, ri);
        }
        scattered = ray_t.create(r_in.origin + hit.t * r_in.direction, final_dir);
        attenuation = material.albedo;
        return true;
    }
    if (material.material_type == material_type_diffuse_light) {
        attenuation = { 0, 0, 0 };
        scattered = {};
        return false;
    }
    scattered = ray_t();
    attenuation = {};
    return false;
}

float3 material_get_emitted(in material_t material, hit_t hit) {
    if (material.material_type == material_type_lambertian) {
        return { 0, 0, 0 };
    }
    if (material.material_type == material_type_metal) {
        return { 0, 0, 0 };
    }
    if (material.material_type == material_type_dielectric) {
        return { 0, 0, 0 };
    }
    if (material.material_type == material_type_diffuse_light) {
        return material.albedo;
    }
    return { 0, 0, 0 };
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 dispatchThreadID: SV_DispatchThreadID) {

    if (dispatchThreadID.x >= uni.width ) return;
    if (dispatchThreadID.y >= uni.height) return;
    
    float4 color = { 0, 0, 0, 1 };

    const float u = float(dispatchThreadID.x) / float(uni.width - 1);
    const float v = 1 - float(dispatchThreadID.y) / float(uni.height - 1);

    uint pixel_index = uint(dispatchThreadID.x + uni.width * (dispatchThreadID.y + uni.height * uni.samples));
    uint seed = pcg_hash(pixel_index);

    if (uni.samples > 2500) return;
    float2 jitter = rand_float2(seed) * 2.f - 1.f;
    jitter /= 1000;
    ray_t ray = ray_t::create(float2(u, v) + jitter, uni.inverse_projection, uni.inverse_view);
    float3 new_color = ray_color(ray, seed);
    float3 current_color = o_color[uint2(dispatchThreadID.x, dispatchThreadID.y)].xyz;

    new_color = (current_color * (uni.samples - 1) + new_color) / float(uni.samples);

    color.xyz = new_color;

    o_color[uint2(dispatchThreadID.x, dispatchThreadID.y)] = color;
}
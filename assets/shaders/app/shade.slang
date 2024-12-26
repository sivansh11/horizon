#include "common.slang"
#include "random.slang"


[vk::push_constant] push_constant_t pc;
[vk::binding(0, 0)] RWTexture2D<float4> o_color;

float4 color(uint32_t id) {
    float4 col = float4(((id * 9665) % 256) / 255.f, ((id * 8765976) % 256) / 255.f, ((id * 2345678) % 256) / 255.f, 1);
    return col;
}
float3 heatmap(float t) {
    const float3 C0 = float3(-0.020390, 0.009557, 0.018508);
    const float3 C1 = float3(3.108226, -0.106297, -1.105891);
    const float3 C2 = float3(-14.539061, -2.943057, 14.548595);
    const float3 C3 = float3(71.394557, 22.644423, -71.418400);
    const float3 C4 = float3(-152.022488, -31.024563, 152.048692);
    const float3 C5 = float3(139.593599, 12.411251, -139.604042);
    const float3 C6 = float3(-46.532952, -0.000874, 46.532928);
    return C0 + (C1 + (C2 + (C3 + (C4 + (C5 + C6 * t) * t) * t) * t) * t) * t;
}

bool near_zero(float3 v) {
    const float s = 1e-8;
    return (abs(v.x) < s) && (abs(v.y) < s) && (abs(v.z) < s);
}

float3 lerp_f3(float3 start, float3 end, float t) {
    return start + (end - start) * t;
}

bool russian_roulette_terminate_ray(inout float3 throughput, inout uint seed) {
    float p = max(throughput.x, max(throughput.y, throughput.z));
    if (rand(seed) > p) {
        return true;
    }
    throughput *= 1 / p;
    return false;
}

[shader("compute")]
[numthreads(work_group_width, work_group_height, 1)]
void compute_main(uint3 dispatch_thread_id: SV_DispatchThreadID, uint group_index: SV_GroupIndex) {
    // i * width + j;
    uint32_t px_i = dispatch_thread_id.x;
    uint32_t px_j = dispatch_thread_id.y;

    uint32_t current_px = px_j * pc.width + px_i;

    if (px_i >= pc.width) return;
    if (px_j >= pc.height) return;

    uint seed = (dispatch_thread_id.x + pc.width * (dispatch_thread_id.y + pc.height * (pc.bounce_id + 1)));

    hit_t hit = pc.hits[current_px];

    const float u = float(px_i) / float(pc.width - 1);
    const float v = 1 - float(px_j) / float(pc.height - 1);

    float3 throughput = pc.throughput[current_px];

    ray_data_t ray_in = pc.ray_datas[current_px];
    if (ray_in.is_trace) {
        if (hit.did_intersect()) {
            // calculate color
            float3 emmision = 0;
            o_color[uint2(px_i, px_j)] += float4(throughput * emmision, 1);
            
            // next ray
            vertex_t vertex = pc.triangles[hit.primitive_id].from_barry(hit.u, hit.v, hit.w);
            float3 N = vertex.normal;
            float3 scattered_direction = vertex.normal + rand_float3_unit_sphere(seed);
            if (near_zero(scattered_direction)) scattered_direction = vertex.normal;
            float3 attenuation = color(hit.primitive_id).xyz;

            pc.throughput[current_px] = throughput * attenuation;
            pc.ray_datas[current_px] = ray_data_t::create(ray_in.origin + hit.t * ray_in.direction, scattered_direction);

            // termiate ray 
            if (russian_roulette_terminate_ray(throughput, seed)) {
                pc.ray_datas[current_px].is_trace = false;
            }

        } else {
            float3 background_color = float3(1, 1, 1);
            o_color[uint2(px_i, px_j)] += float4(throughput * background_color, 1);
            pc.ray_datas[current_px].is_trace = false;
        }
    }
}
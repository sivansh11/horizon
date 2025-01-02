#include "common.slang"
#include "random.slang"


[vk::push_constant] push_constant_t pc;
[vk::binding(0, 0)] RWTexture2D<float3> o_color;

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
[numthreads(64, 1, 1)]
void compute_main(uint3 dispatch_thread_id: SV_DispatchThreadID, uint group_index: SV_GroupIndex) {
    uint32_t id = dispatch_thread_id.x;
    if (id >= pc.width * pc.height) return;
    if (id >= *pc.num_rays) return;

    hit_t hit = pc.hits[id];
    ray_data_t ray_in = pc.ray_datas[id];

    uint32_t px_i = ray_in.px_id % pc.width;
    uint32_t px_j = ray_in.px_id / pc.width;

    uint seed = (px_i + pc.width * (px_j + pc.height * (pc.bounce_id + 1)));

    float3 throughput = pc.throughput[ray_in.px_id];
    if (hit.did_intersect()) {
        // calculate color
        float3 emmision = 0;
        o_color[uint2(px_i, px_j)] += throughput * emmision;
        
        // next ray
        bool shouldAppendRay = !russian_roulette_terminate_ray(throughput, seed);
        uint32_t laneAppendOffset = WavePrefixCountBits(shouldAppendRay);
        uint32_t appendCount = WaveActiveCountBits(shouldAppendRay);

        uint32_t appendOffset;
        if (WaveIsFirstLane()) {
            InterlockedAdd(*pc.new_num_rays, appendCount, appendOffset);
        }
        appendOffset = WaveReadLaneFirst(appendOffset);
        if (shouldAppendRay) {
            vertex_t vertex = pc.triangles[hit.primitive_id].from_barry(hit.u, hit.v, hit.w);
            float3 N = vertex.normal;
            bool front_face = dot(ray_in.direction, N) < 0;
            N = front_face ? N : -N;
            float3 scattered_direction = N + rand_float3_unit_sphere(seed);
            float3 reflected_direction = reflect(normalize(ray_in.direction), N);
            if (near_zero(scattered_direction)) scattered_direction = vertex.normal;
            float3 attenuation = color(hit.primitive_id).xyz;
            uint32_t index = appendOffset + laneAppendOffset;
            ray_data_t new_ray_data = ray_data_t::create(ray_in.origin + hit.t * ray_in.direction, scattered_direction /* reflected_direction */, ray_in.px_id);
            pc.new_ray_datas[index] = new_ray_data;
            pc.throughput[ray_in.px_id] = throughput * attenuation;
        }

        // // only add ray if not termiated
        // if (!russian_roulette_terminate_ray(throughput, seed)) {
        //     uint32_t index;
        //     InterlockedAdd(*pc.num_rays, 1, index);
        //     ray_data_t new_ray_data = ray_data_t::create(ray_in.origin + hit.t * ray_in.direction, scattered_direction, ray_in.px_id);
        //     pc.new_ray_datas[index] = new_ray_data;
        //     pc.throughput[ray_in.px_id] = throughput * attenuation;
        // }
    } else {
        float3 background_color = float3(1, 1, 1);
        o_color[uint2(px_i, px_j)] += throughput * background_color;
    }
}
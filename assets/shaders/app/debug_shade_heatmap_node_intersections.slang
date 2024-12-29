#include "common.slang"

[vk::push_constant] push_constant_t pc;
[vk::binding(0, 0)] RWTexture2D<float3> o_color;

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

    if (hit.did_intersect()) {
        // calculate color
        o_color[uint2(px_i, px_j)] = heatmap(hit.node_intersection_count / 100.f);
    } else {
        o_color[uint2(px_i, px_j)] = float3(0, 0, 0);
    }
}
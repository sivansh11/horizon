#include "common.slang"


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

[shader("compute")]
[numthreads(work_group_width, work_group_height, 1)]
void compute_main(uint3 dispatch_thread_id: SV_DispatchThreadID, uint group_index: SV_GroupIndex) {
    // i * width + j;
    uint32_t px_i = dispatch_thread_id.x;
    uint32_t px_j = dispatch_thread_id.y;

    if (px_i >= pc.width) return;
    if (px_j >= pc.height) return;

    hit_t hit = pc.hits[px_j * pc.width + px_i];

    const float u = float(px_i) / float(pc.width - 1);
    const float v = 1 - float(px_j) / float(pc.height - 1);

    if (hit.did_intersect()) {
        o_color[uint2(px_i, px_j)] = color(hit.primitive_id);
        // o_color[uint2(px_i, px_j)] = float4(heatmap(hit.node_intersection_count / 100.f), 1);
    } else {
        o_color[uint2(px_i, px_j)] = float4(u, v, 0, 1);
    }
}
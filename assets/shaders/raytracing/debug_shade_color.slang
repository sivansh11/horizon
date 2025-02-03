#include "common.slang"

[vk::push_constant] push_constant_t pc;
[vk::binding(0, 0)] RWTexture2D<float3> o_color;

float4 color(uint32_t id) {
    float4 col = float4(((id * 9665) % 256) / 255.f, ((id * 8765976) % 256) / 255.f, ((id * 2345678) % 256) / 255.f, 1);
    return col;
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
        o_color[uint2(px_i, px_j)] = color(hit.primitive_id).xyz;
    } else {
        o_color[uint2(px_i, px_j)] = float3(0, 0, 0);
    }
}
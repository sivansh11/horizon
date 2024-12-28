#include "common.slang"

[vk::push_constant] push_constant_t pc;
[vk::binding(0, 0)] RWTexture2D<float4> o_color;

[shader("compute")]
[numthreads(work_group_width, work_group_height, 1)]
void compute_main(uint3 dispatch_thread_id: SV_DispatchThreadID, uint group_index: SV_GroupIndex) {
    // i * width + j;
    uint32_t px_i = dispatch_thread_id.x;
    uint32_t px_j = dispatch_thread_id.y;

    if (px_i >= pc.width) return;
    if (px_j >= pc.height) return;

    o_color[uint2(px_i, px_j)] = float4(0, 0, 0, 0);
    pc.throughput[px_j * pc.width + px_i] = float3(1, 1, 1);
}
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

    const float u = float(px_i) / float(pc.width - 1);
    const float v = 1 - float(px_j) / float(pc.height - 1);

    ray_data_t ray_data = pc.camera.ray_gen(float2(u, v));
    pc.ray_datas[px_j * pc.width + px_i] = ray_data;

    o_color[uint2(px_i, px_j)] = float4(0, 0, 0, 0);
    pc.throughput[px_j * pc.width + px_i] = float3(1, 1, 1);

    if (px_i == 0 && px_j == 0) {
        *pc.num_rays = pc.width * pc.height;
        pc.dispatch_indirect_cmd.x = (pc.width + 8 - 1) / 8;
        pc.dispatch_indirect_cmd.y = (pc.height + 8 - 1) / 8;
        pc.dispatch_indirect_cmd.z = 1;
    }
}

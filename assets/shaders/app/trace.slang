#include "common.slang"



[vk::push_constant] push_constant_t pc;

[shader("compute")]
[numthreads(32, 1, 1)]
void compute_main(uint3 dispatch_thread_id: SV_DispatchThreadID, uint group_index: SV_GroupIndex) {
    uint32_t id = dispatch_thread_id.x;
    // i * width + j;
    uint32_t px_i = id % pc.width;
    uint32_t px_j = id / pc.width;

    if (px_i >= pc.width) return;
    if (px_j >= pc.height) return;

    ray_data_t ray_data = pc.ray_datas[px_j * pc.width + px_i];
    hit_t hit = intersect(*pc.bvh, ray_data, pc.triangles, group_index);
    pc.hits[px_j * pc.width + px_i] = hit;
}

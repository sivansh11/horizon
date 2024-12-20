#include "common.slang"



[vk::push_constant] push_constant_t pc;

[shader("compute")]
[numthreads(work_group_width, work_group_height, 1)]
void compute_main(uint3 dispatch_thread_id: SV_DispatchThreadID, uint group_index: SV_GroupIndex) {
    // i * width + j;
    uint32_t px_i = dispatch_thread_id.x;
    uint32_t px_j = dispatch_thread_id.y;

    if (px_i >= pc.width) return;
    if (px_j >= pc.height) return;

    ray_data_t ray_data = pc.ray_datas[px_j * pc.width + px_i];
    hit_t hit = intersect(*pc.bvh, ray_data, pc.triangles, group_index);
    pc.hits[px_j * pc.width + px_i] = hit;
}

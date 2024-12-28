#include "common.slang"

[vk::push_constant] push_constant_t pc;

[shader("compute")]
// [numthreads(work_group_width, work_group_height, 1)]
[numthreads(64, 1, 1)]
void compute_main(uint3 dispatch_thread_id: SV_DispatchThreadID, uint group_index: SV_GroupIndex) {
    // // i * width + j;
    // uint32_t px_i = dispatch_thread_id.x;
    // uint32_t px_j = dispatch_thread_id.y;
    uint32_t id = dispatch_thread_id.x;

    if (id >= *pc.num_rays) return;
    if (id >= pc.width * pc.height) return;

    if (id == 0) {
        *pc.new_num_rays = 0;
    }

    ray_data_t ray_data = pc.ray_datas[id];

    hit_t hit = intersect(*pc.bvh, ray_data, pc.triangles, group_index);
    pc.hits[id] = hit;
}
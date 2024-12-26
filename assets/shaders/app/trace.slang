#include "common.slang"



[vk::push_constant] push_constant_t pc;

[shader("compute")]
[numthreads(work_group_width, work_group_height, 1)]
void compute_main(uint3 dispatch_thread_id: SV_DispatchThreadID, uint group_index: SV_GroupIndex) {
    // // reset num_rays to 0
    // if (id == 0) {
    //     *pc.num_rays = 0;
    // }
    // i * width + j;
    uint32_t px_i = dispatch_thread_id.x;
    uint32_t px_j = dispatch_thread_id.y;

    if (px_i >= pc.width) return;
    if (px_j >= pc.height) return;

    ray_data_t ray_data = pc.ray_datas[px_j * pc.width + px_i];

    if (!ray_data.should_trace) return;

    hit_t hit = intersect(*pc.bvh, ray_data, pc.triangles, group_index);
    pc.hits[px_j * pc.width + px_i] = hit;

    // // generate new rays
    // if (false) {
    //     uint32_t ray_id;
    //     InterlockedAdd(*pc.num_rays, 1, ray_id);
    //     pc.ray_datas[ray_id];
    // }
    // // set indirect dispatch command
    // if (id == 0) {
    //     pc.dispatch_indirect_cmd.x = ((*pc.num_rays) + 32 - 1) / 32;
    //     pc.dispatch_indirect_cmd.y = 1;
    //     pc.dispatch_indirect_cmd.z = 1;
    // }
}

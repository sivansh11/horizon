#include "../raytracing/raytrace_core.slang"

struct DispatchIndirectCmd {
    uint32_t x, y, z;
}

struct push_constant_t {
    float3 *throughput;
    uint32_t *num_rays;
    DispatchIndirectCmd *dispatch_indirect_cmd;
    ray_data_t *ray_datas;
    bvh_t *bvh;
    triangle_t *triangles;
    camera_t *camera;
    hit_t *hits;
    uint32_t width;
    uint32_t height;
    uint32_t bounce_id;
};


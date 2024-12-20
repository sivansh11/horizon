#include "../raytracing/raytrace_core.slang"
struct push_constant_t {
    ray_data_t *ray_datas;
    bvh_t *bvh;
    triangle_t *triangles;
    camera_t *camera;
    hit_t *hits;
    uint32_t width;
    uint32_t height;
};


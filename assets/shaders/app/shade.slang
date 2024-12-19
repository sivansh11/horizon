#include "../raytracing/raytrace_core.slang"

struct push_constant_t {
    bvh_t *bvh;
    triangle_t *triangles;
    camera_t *camera;
    hit_t *hits;
    uint32_t width;
    uint32_t height;
};

[vk::push_constant] push_constant_t pc;
[vk::binding(0, 0)] RWTexture2D<float4> o_color;

float4 color(uint32_t id) {
    float4 col = float4(((id * 9665) % 256) / 255.f, ((id * 8765976) % 256) / 255.f, ((id * 2345678) % 256) / 255.f, 1);
    return col;
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
    } else {
        o_color[uint2(px_i, px_j)] = float4(u, v, 0, 1);
    }
}
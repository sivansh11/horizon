#include "../raytracing/raytrace_core.slang"

struct push_constant_t {
    ray_t *ray;
    triangle_t *triangle;
    uint32_t *res;
};

[vk::push_constant] push_constant_t pc;

[shader("compute")]
[numthreads(1, 1, 1)]
void compute_main(uint3 dispatch_thread_id: SV_DispatchThreadID) {
    printf("ray:\n\t%v3f\n\t%v3f\n", pc.ray.origin, pc.ray.direction);
    printf("triangle:\n\t%v3f\n\t%v3f\n\t%v3f\n", pc.triangle.v0, pc.triangle.v1, pc.triangle.v2);
    if (triangle_intersect(ray_data_t::create(*pc.ray), *pc.triangle).did_intersect()) {
        printf("1");
        *pc.res = 1;
    } else {
        printf("0");
        *pc.res = 0;
    }
    printf("\n");
}

#include "my_types.slang"

const static float infinity = 1000000000;

float safe_inverse(float x) {
    const float epsilon = 0.001;
    if (abs(x) <= epsilon) {
        if (x >= 0) return 1.f / epsilon;
        return -1.f / epsilon;
    }
    return 1.f / x;
}

struct ray_t {
    static ray_t create(float3 origin, float3 direction) {
        ray_t ray;
        ray.origin = origin;
        ray.direction = direction;
        ray.tmin = 0.0001;
        ray.tmax = infinity;
        ray.inverse_direction = { safe_inverse(direction.x), safe_inverse(direction.y), safe_inverse(direction.z) };
        return ray;
    }

    static ray_t create(in float2 uv, in float4x4 inv_projection, in float4x4 inv_view) {
        float2 px_nds = uv * 2.f - 1.f;
        float3 point_nds = float3(px_nds, -1);
        float4 point_ndsh = float4(point_nds, 1);
        float4 dir_eye = inv_projection * point_ndsh;
        dir_eye.w = 0;
        float3 dir_world = float3(inv_view * dir_eye);
        float3 eye = { inv_view[0][3], inv_view[1][3], inv_view[2][3] };
        ray_t ray = ray_t::create(eye, dir_world);
        return ray;
    }

    float3 origin, direction, inverse_direction;
    float tmin, tmax;
};
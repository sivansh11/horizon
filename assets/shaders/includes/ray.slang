float safe_inverse(float x) {
    const float epsilon = 0.001;
    if (abs(x) <= epsilon) {
        if (x >= 0) return 1.f / epsilon;
        return -1.f / epsilon;
    }
    return 1.f / x;
}

static const float infinity = 1000000000;

struct ray_t {
    static ray_t create(float3 origin, float3 direction) {
        ray_t ray;
        ray.origin = origin;
        ray.direction = direction;
        ray.tmin = 0;
        ray.tmax = infinity;
        ray.inverse_direction = { safe_inverse(direction.x), safe_inverse(direction.y), safe_inverse(direction.z) };
        return ray;
    }

    float3 origin, direction, inverse_direction;
    float tmin, tmax;
};
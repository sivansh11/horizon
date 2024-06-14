import ray;

struct triangle_intersection_t {
    bool did_intersect() { return intersection; }
    bool intersection;
    float t;
};

struct triangle_t {
    triangle_intersection_t intersect(in ray_t ray) {
        float3 e1 = v0 - v1;
        float3 e2 = v2 - v0;
        float3 n = cross(e1, e2);

        float3 c = v0 - ray.origin;
        float3 r = cross(ray.direction, c);
        float inverse_det = 1.0f / dot(n, ray.direction);

        float u = dot(r, e2) * inverse_det;
        float v = dot(r, e1) * inverse_det;
        float w = 1.0f - u - v;

        if (u >= 0 && v >= 0 && w >= 0) {
            float t = dot(n, c) * inverse_det;
            if (t >= ray.tmin && t <= ray.tmax) {
                // ray.tmax = t;
                triangle_intersection_t intersection;
                intersection.intersection = true;
                intersection.t = t;
                return intersection;
            }
        }
        triangle_intersection_t intersection;
        intersection.intersection = false;
        intersection.t = infinity;
        return intersection;
    }

    float3 v0, v1, v2;
};
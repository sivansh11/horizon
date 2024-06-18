#include "common.slang"
#include "my_types.slang"
#include "ray.slang"

struct sphere_intersection_t {
    bool did_intersect() { return intersection; }
    bool intersection = false;
    float t = -1;
}

struct sphere_t {
    sphere_intersection_t intersect(in ray_t ray) {
        sphere_intersection_t intersection;
        const float3 oc = center - ray.origin;
        const float a = length2(ray.direction);
        const float h = dot(ray.direction, oc);
        const float c = length2(oc) - radius * radius;
        const float discriminant = h * h - a * c;

        if (discriminant < 0) return intersection;

        const float sqrtd = sqrt(discriminant);
        if (sqrtd < 0) return intersection;

        float root = (h - sqrtd) / a;
        if (root <= ray.tmin || root >= ray.tmax) {
            root = (h + sqrtd) / a;
            if (root <= ray.tmin || root >= ray.tmax) return intersection;
        }

        intersection.intersection = true;
        intersection.t = root;
        return intersection;
    }

    vec3 center;
    float radius;
    uint32_t material_id;
};
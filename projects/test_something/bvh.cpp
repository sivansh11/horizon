#include "bvh.hpp"

namespace core {

namespace bvh {

void ray_intersect_triangle(ray_t& ray, const triangle_t& triangle) {
    const glm::vec3 edge1 = triangle.vertex1 - triangle.vertex0;
    const glm::vec3 edge2 = triangle.vertex2 - triangle.vertex0;
    const glm::vec3 h = glm::cross( ray.direction, edge2 );
    const float a = glm::dot( edge1, h );
    if (a > -0.0001f && a < 0.0001f) return; // ray parallel to triangle
    const float f = 1 / a;
    const glm::vec3 s = ray.origin - triangle.vertex0;
    const float u = f * glm::dot( s, h );
    if (u < 0 || u > 1) return;
    const glm::vec3 q = cross( s, edge1 );
    const float v = f * glm::dot( ray.direction, q );
    if (v < 0 || u + v > 1) return;
    const float t = f * dot( edge2, q );
    if (t > 0.0001f) ray.t = glm::min( ray.t, t );
}

} // namespace bvh

} // namespace core

#ifndef BVH_HPP
#define BVH_HPP

#include <glm/glm.hpp>

#include <vector>
#include <limits>

// https://jacco.ompf2.com/2022/04/13/how-to-build-a-bvh-part-1-basics/

namespace core {

namespace bvh {
    
struct triangle_t {
    glm::vec3 vertex0;
    glm::vec3 vertex1;
    glm::vec3 vertex2;
    glm::vec3 centroid;
};

constexpr float infinity = std::numeric_limits<float>::max();

struct ray_t {
    glm::vec3 origin, direction;
    float t = infinity;
};

void ray_intersect_triangle(ray_t& ray, const triangle_t& triangle);

struct aabb_t {
    glm::vec3 min, max;
};

struct node_t {
    aabb_t node_bound;
    uint32_t left_node, first_triangle_index, triangle_count;
    bool is_leaf() const { return triangle_count > 0; }
};

} // namespace bvh

} // namespace core

#endif
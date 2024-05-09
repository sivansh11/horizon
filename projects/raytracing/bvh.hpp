#ifndef bvh_hpp
#define bvh_hpp

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <cstdint>
#include <algorithm>
#include <limits>
#include <vector>
#include <numeric>
#include <cassert>
#include <array>
#include <stack>
#include <utility>
#include <tuple>
#include <cmath>
#include <cctype>
#include <optional>
#include <fstream>
#include <iostream>


inline float robust_min(float a, float b);
inline float robust_max(float a, float b);
inline float safe_inverse(float x);

struct bounding_box_t {

    int largest_axis() const;
    float half_area() const;
    static const bounding_box_t empty();
    bounding_box_t& extend(const bounding_box_t& other);
    glm::vec3 diagonal() const;
    bounding_box_t& extend(const glm::vec3& point);
    
    glm::vec3 min, max;
};

struct ray_t {
    glm::vec3 inverse_direction() const;

    glm::vec3 origin, direction;
    float tmin, tmax;
};

struct hit_t {
    uint32_t primitive_index;
    operator bool() const { return primitive_index != static_cast<uint32_t>(-1); }
    static hit_t none() { return hit_t{static_cast<uint32_t>(-1)}; }
};

struct triangle_t {

    bool intersect(ray_t& ray) const;

    glm::vec3 p0, p1, p2;
};

struct node_t {

    // struct intersection_t {
    //     float tmin;
    //     float tmax;
    //     operator bool() const { return tmin <= tmax; }
    // };

    bool intersect(const ray_t& ray) const;

    bool is_leaf() const { return primitive_count != 0; }

    bounding_box_t bounding_box{};
    uint32_t primitive_count{};
    uint32_t first_index{};
};



struct bvh_t {

    static bvh_t build(const bounding_box_t *bounding_boxes, const glm::vec3 *centers, size_t primitive_count);

    uint32_t depth(uint32_t node_index = 0) const;

    template <typename primitive>
    hit_t traverse(ray_t& ray, const std::vector<primitive>& primitives) const {
        hit_t hit = hit_t::none();
        std::stack<uint32_t> stack;
        stack.push(0);

        while (!stack.empty()) {
            auto& node = nodes[stack.top()];
            stack.pop();
            if (!node.intersect(ray)) 
                continue;

            if (node.is_leaf()) {
                for (uint32_t i = 0; i < node.primitive_count; i++) {
                    uint32_t primitive_index = primitive_indices[node.first_index + i];
                    if (primitives[primitive_index].intersect(ray))    
                        hit.primitive_index = primitive_index;
                }
            } else {
                stack.push(node.first_index);
                stack.push(node.first_index + 1);
            }
        }
        return hit;
    }

    std::vector<node_t> nodes;
    std::vector<uint32_t> primitive_indices;

private:

    struct build_config_t {
        uint32_t min_primitives = 2;
        uint32_t max_primitives = 128;
        float traversal_cost = 10.0f;
        uint32_t bin_count = 16;
    };

    struct bin_t {

        bin_t& extend(const bin_t& other);
        static uint32_t bin_index(int axis, const bounding_box_t& aabb, const glm::vec3& center);

        float cost() const { return aabb.half_area() * primitive_count; }

        bounding_box_t aabb = bounding_box_t::empty();
        uint32_t primitive_count = 0;
    };

    struct split_t {

        operator bool() const { return right_bin != 0; }
        bool operator < (const split_t& other) const {
            return *this && cost < other.cost;
        }

        static split_t find_best_split(int axis, const bvh_t& bvh, const node_t& node, const bounding_box_t *aabbs, const glm::vec3 *centers);

        int axis = 0;
        float cost = std::numeric_limits<float>::max();
        uint32_t right_bin = 0;
    };

    static const build_config_t build_config;
    
    static void build_recursive(bvh_t& bvh, uint32_t node_index, uint32_t& node_count, const bounding_box_t *aabbs, const glm::vec3 *centers);
};

#endif
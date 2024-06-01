#ifndef NEW_BVH_HPP
#define NEW_BVH_HPP

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <vector>

namespace bvh {

struct aabb_t {
    aabb_t& extend(const aabb_t& other) {
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
        return *this;
    }

    glm::vec3 min, max;
};

struct primitive_t {
    aabb_t aabb;
    glm::vec3 center;
};

struct node_t {
    aabb_t aabb;
    node_t *left, *right;
    bool is_leaf;
    std::vector<primitive_t> primitives;
};

struct bvh_t {
    
    bvh_t(const aabb_t *aabbs, const glm::vec3 *centers, size_t primitive_count) {
        root_node = new node_t;

        for (size_t i = 0; i < primitive_count; i++) {
            root_node->primitives.emplace_back(aabbs[i], centers[i]);
        }

        update_node_bounds(root_node);
        subdivide(root_node);
    }

    void update_node_bounds(node_t *node) {
        node->aabb.min = glm::vec3{ std::numeric_limits<float>::max() };
        node->aabb.max = glm::vec3{ -std::numeric_limits<float>::max() };
        for (auto& primitive : node->primitives) {
            node->aabb.extend(primitive.aabb);
        }
    }

    void subdivide(node_t *node) {
        if (node->primitives.size() <= 2) return;

        node->left = new node_t;
        node->right = new node_t;

        // choose the largest axis of the node
        glm::vec3 extent = node->aabb.max - node->aabb.min;
        uint32_t axis = 0;
        if (extent.y > extent.x) axis = 1;
        if (extent.z > extent[axis]) axis = 2;

        float split_pos = node->aabb.min[axis] + extent[axis] * 0.5;  // choose the mid of that largest axis
        
        // based on the largest axis, if the centroid of the trianlge lies in the left side of the mid point, put it in the left node, otherwise put it in the right node
        for (auto& primitive : node->primitives) {
            if (primitive.center[axis] < split_pos) {
                node->left->primitives.push_back(primitive);
            } else {
                node->right->primitives.push_back(primitive);
            }
        }

        // remove all primitives from the current node
        node->primitives.clear();

        update_node_bounds(node->left);
        update_node_bounds(node->right);

        subdivide(node->left);
        subdivide(node->right);
    }

    // static void update_node_bounds(node_t *node, const aabb_t *aabbs, const glm::vec3 *centers, size_t primitive_count) {
    //     node->aabb.min = glm::vec3{ std::numeric_limits<float>::max()};
    //     node->aabb.max = glm::vec3{ -std::numeric_limits<float>::max()};
        
    // }

    // static bvh_t build(const aabb_t *aabbs, const glm::vec3 *centers, size_t primitive_count) {
    //     bvh_t bvh{};
    //     bvh.root_node = new node_t;

        

    //     update_node_bounds(bvh.root_node, aabbs, centers, primitive_count);

    //     return bvh;
    // }

    node_t *root_node;
};

} // namespace bvh

#endif
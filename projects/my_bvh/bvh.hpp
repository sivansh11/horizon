#ifndef bvh_hpp
#define bvh_hpp

#include "core/core.hpp"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <vector>
#include <stack>
#include <optional>

namespace utils {

float safe_inverse(float x) {
    const float epsilon = std::numeric_limits<float>::epsilon();
    if (std::abs(x) <= epsilon) {
        if (x >= 0) return 1.f / epsilon;
        return -1.f / epsilon;
    }
    return 1.f / x;
}

} // namespace utils

namespace new_bvh {

struct ray_t {
    ray_t(const glm::vec3& origin, const glm::vec3& direction) : origin(origin), direction(direction), inverse_direction(utils::safe_inverse(direction.x), utils::safe_inverse(direction.y), utils::safe_inverse(direction.z)), tmin(0), tmax(std::numeric_limits<float>::max()) {}

    glm::vec3 origin, direction, inverse_direction;
    float tmin, tmax;
};

struct aabb_t {
    aabb_t() : min(std::numeric_limits<float>::max()), max(-std::numeric_limits<float>::max()) {}

    aabb_t& extend(const aabb_t& other) {
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
        return *this;
    }

    aabb_t& extend(const glm::vec3& point) {
        min = glm::min(min, point);
        max = glm::max(max, point);
        return *this;
    }

    bool intersect(const ray_t& ray) const {
        glm::vec3 tmin = (min - ray.origin) * ray.inverse_direction;
        glm::vec3 tmax = (max - ray.origin) * ray.inverse_direction;

        glm::vec3 old_tmin = tmin;
        glm::vec3 old_tmax = tmax;

        tmin = glm::min(old_tmin, old_tmax);
        tmax = glm::max(old_tmin, old_tmax);

        float _tmin = glm::max(tmin[0], glm::max(tmin[1], glm::max(tmin[2], ray.tmin)));
        float _tmax = glm::min(tmax[0], glm::min(tmax[1], glm::min(tmax[2], ray.tmax)));

        return _tmin <= _tmax;
    }

    glm::vec3 min, max;
};

struct triangle_t {

    bool intersect(ray_t& ray) const {
        glm::vec3 e1 = p0 - p1;
        glm::vec3 e2 = p2 - p0;
        glm::vec3 n = glm::cross(e1, e2);

        glm::vec3 c = p0 - ray.origin;
        glm::vec3 r = glm::cross(ray.direction, c);
        float inverse_det = 1.0f / glm::dot(n, ray.direction);

        float u = glm::dot(r, e2) * inverse_det;
        float v = glm::dot(r, e1) * inverse_det;
        float w = 1.0f - u - v;

        if (u >= 0 && v >= 0 && w >= 0) {
            float t = glm::dot(n, c) * inverse_det;
            if (t >= ray.tmin && t <= ray.tmax) {
                ray.tmax = t;
                return true;
            }
        }
        return false;
    }

    glm::vec3 p0, p1, p2;
    glm::vec3 color;
};

struct node_t {
    aabb_t aabb;
    uint32_t primitive_count;
    uint32_t first_index;
};

struct bvh_t {
    bvh_t(const aabb_t *aabbs, const glm::vec3 *centers, size_t primitive_count) {
        primitive_indices.resize(primitive_count);
        for (uint32_t i = 0; i < primitive_count; i++) primitive_indices[i] = i;

        nodes.resize(2 * primitive_count - 1);
        nodes[0].primitive_count = primitive_count;
        nodes[0].first_index = 0;

        uint32_t node_count = 1;
        update_node_bounds(0, aabbs, centers);
        subdivide_node(0, node_count, aabbs, centers);
    }

    void update_node_bounds(uint32_t node_id, const aabb_t *aabbs, const glm::vec3 *centers) {
        node_t& node = nodes[node_id];
        for (uint32_t i = 0; i < node.primitive_count; i++) node.aabb.extend(centers[primitive_indices[i]]);
    }

    void subdivide_node(uint32_t node_id, uint32_t& node_count, const aabb_t *aabbs, const glm::vec3 *centers) {
        node_t& node = nodes[node_id];
        if (node.primitive_count <= 2) return;  // 2 is configurable

        glm::vec3 extent = node.aabb.max - node.aabb.min;
        uint32_t axis = 0;
        if (extent.y > extent.x) axis = 1;
        if (extent.z > extent[axis]) axis = 2;

        // mid of largest axis
        float split_pos = node.aabb.min[axis] + extent[axis] * 0.5f;

        // partition
        uint32_t i = node.first_index;  // first index
        uint32_t j = i + node.primitive_count - 1;   // last index
        check(i <= j, "i needs to be greater than or equal to j, something is wrong");
        while (i <= j) {
            if (centers[primitive_indices[i]][axis] < split_pos) i++;  // correct spot
            else {
                // else swap with the last and decrement it
                std::swap(primitive_indices[i], primitive_indices[j]);
                j--;
            }
        }
        uint32_t left_count = i - node.first_index;
        if (left_count == 0 || left_count == node.primitive_count) return;

        

        node_t& left_node = nodes[node_count++];
        node_t& right_node = nodes[node_count++];
    }

    std::vector<node_t> nodes;
    std::vector<uint32_t> primitive_indices;
};

} // namespace new_bvh


namespace bvh {

struct ray_t {
    glm::vec3 inverse_direction() const {
        return glm::vec3{ utils::safe_inverse(direction.x), utils::safe_inverse(direction.y), utils::safe_inverse(direction.z) };
    }

    glm::vec3 origin, direction;
    float tmin, tmax;
};

struct aabb_t {
    aabb_t& extend(const aabb_t& other) {
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
        return *this;
    }

    aabb_t& extend(const glm::vec3& point) {
        min = glm::min(min, point);
        max = glm::max(max, point);
        return *this;
    }

    float area() const {
        glm::vec3 extent = max - min;
        return extent.x * extent.y + extent.y * extent.z + extent.z * extent.x;
    }

    bool intersect(const ray_t& ray) const {
        const glm::vec3 inverse_direction = ray.inverse_direction();
        glm::vec3 tmin = (min - ray.origin) * inverse_direction;
        glm::vec3 tmax = (max - ray.origin) * inverse_direction;

        glm::vec3 old_tmin = tmin;
        glm::vec3 old_tmax = tmax;

        tmin = glm::min(old_tmin, old_tmax);
        tmax = glm::max(old_tmin, old_tmax);

        float _tmin = glm::max(tmin[0], glm::max(tmin[1], glm::max(tmin[2], ray.tmin)));
        float _tmax = glm::min(tmax[0], glm::min(tmax[1], glm::min(tmax[2], ray.tmax)));

        return _tmin <= _tmax;
    }

    glm::vec3 min = glm::vec3{ std::numeric_limits<float>::max() };
    glm::vec3 max = glm::vec3{ -std::numeric_limits<float>::max() };
};

struct triangle_t {

    bool intersect(ray_t& ray) const {
        glm::vec3 e1 = p0 - p1;
        glm::vec3 e2 = p2 - p0;
        glm::vec3 n = glm::cross(e1, e2);

        glm::vec3 c = p0 - ray.origin;
        glm::vec3 r = glm::cross(ray.direction, c);
        float inverse_det = 1.0f / glm::dot(n, ray.direction);

        float u = glm::dot(r, e2) * inverse_det;
        float v = glm::dot(r, e1) * inverse_det;
        float w = 1.0f - u - v;

        if (u >= 0 && v >= 0 && w >= 0) {
            float t = glm::dot(n, c) * inverse_det;
            if (t >= ray.tmin && t <= ray.tmax) {
                ray.tmax = t;
                return true;
            }
        }
        return false;
    }

    glm::vec3 p0, p1, p2;
    glm::vec3 color;
};

struct primitive_t {
    aabb_t aabb;
    glm::vec3 center;
    triangle_t triangle;
};

struct node_t {
    bool is_leaf() const { return !left && !right && primitives.size(); }
    aabb_t aabb;
    node_t *left = nullptr, *right = nullptr;
    std::vector<primitive_t> primitives;
};

struct hit_t {
    std::optional<primitive_t> primitive = std::nullopt;
};

struct bvh_t {
    bvh_t(const std::vector<primitive_t>& primitives) {
        root_node = new node_t();
        for (auto& primitive : primitives) root_node->primitives.push_back(primitive);
        update_node_bounds(root_node);
        subdivide_node(root_node);
    }

    ~bvh_t() {
        std::stack<node_t *> stack;
        stack.push(root_node);
        while (stack.size()) {
            node_t *node = stack.top();
            stack.pop();
            if (!node->is_leaf()) {
                if (node->left) stack.push(node->left);
                if (node->right) stack.push(node->right);
            }
            delete node;
        }
    }

    void update_node_bounds(node_t *node) {
        for (auto& primitive : node->primitives) node->aabb.extend(primitive.aabb);
    }

    std::pair<uint32_t, float> choose_axis_and_split_pos_basic(node_t *node) {
        // get the largest axis of the node aabb
        glm::vec3 extent = node->aabb.max - node->aabb.min;
        uint32_t axis = 0;
        if (extent.y > extent.x) axis = 1;
        if (extent.z > extent[axis]) axis = 2;

        // mid of largest axis
        float mid = node->aabb.min[axis] + extent[axis] * 0.5f;
        return {axis, mid};
    }

    std::pair<uint32_t, float> choose_axis_and_split_pos_sah(node_t *node) {
        auto evaluate_sah = [](node_t *node, uint32_t axis, float position) -> float {
            aabb_t left_box, right_box;
            uint32_t left_count = 0, right_count = 0;
            for (auto& primitive : node->primitives) {
                if (primitive.center[axis] < position) {
                    left_count++;
                    left_box.extend(primitive.triangle.p0);
                    left_box.extend(primitive.triangle.p1);
                    left_box.extend(primitive.triangle.p2);
                } else {
                    right_count++;
                    right_box.extend(primitive.triangle.p0);
                    right_box.extend(primitive.triangle.p1);
                    right_box.extend(primitive.triangle.p2);
                }
            }
            float cost = left_count * left_box.area() + right_count * right_box.area();
            return cost > 0 ? cost : std::numeric_limits<float>::max();
        };

        uint32_t best_axis = static_cast<uint32_t>(-1);
        float best_position = 0;
        // TODO: add cost threashold for faster returns
        float best_cost = std::numeric_limits<float>::max();

        for (uint32_t axis = 0; axis < 3; axis++) for (auto& primitive : node->primitives) {
            float position = primitive.center[axis];
            float cost = evaluate_sah(node, axis, position);
            if (cost < best_cost) {
                best_cost = cost;
                best_position = position;
                best_axis = axis;
            }
        }
        return { best_axis, best_position };
    }

    void subdivide_node(node_t *node) {
        // min number of primitives, dont subdivide if lessthan equal to min 
        if (node->primitives.size() <= 2) return;  // 2 is configurable

        // create child nodes
        node->left = new node_t();
        node->right = new node_t();

        // split point and axis choosing
        auto [axis, split_pos] = choose_axis_and_split_pos_basic(node);

        // based on the largest axis mid, if the center of the primitive on the axis is less than mid, put primitive in left child, else in right child
        for (auto& primitive : node->primitives) {
            if (primitive.center[axis] < split_pos) node->left->primitives.push_back(primitive);
            else node->right->primitives.push_back(primitive);
        }

        // since the primitives have been divided between the left and right nodes, clear the current node's primitives
        node->primitives.clear();

        // if any child doesnt have any primitive, ie, the other child has all the primitives, this will cause an infinite recursion, so exit
        if (node->left->primitives.size() == 0 || node->right->primitives.size() == 0) return;

        // update and subdivide children
        update_node_bounds(node->left);
        update_node_bounds(node->right);

        subdivide_node(node->left);
        subdivide_node(node->right);
    }

    uint32_t node_count() {
        std::stack<node_t *> stack;
        stack.push(root_node);

        uint32_t count = 0;

        while (stack.size()) {
            node_t *node = stack.top();
            stack.pop();
            count++;
            if (node->left || node->right) {
                stack.push(node->left);
                stack.push(node->right);
            }
        }
        return count;
    }

    std::tuple<hit_t, float, float> stack_traverse(ray_t& ray) const {
        hit_t hit{};
        std::stack<node_t *> stack{};
        stack.push(root_node);
        float node_intersections = 0;
        float primtive_intersections = 0;
        while (stack.size()) {
            node_t *node = stack.top();
            stack.pop();
            node_intersections += 1;
            if (!node->aabb.intersect(ray)) continue;
            if (!node->left && !node->right) {
                for (auto& primitive : node->primitives) {
                    primtive_intersections += 1;
                    if (primitive.triangle.intersect(ray)) {
                        hit.primitive = primitive;
                    }
                }
            } else {
                stack.push(node->left);
                stack.push(node->right);
            }
        }
        return { hit, node_intersections, primtive_intersections };
    }

    node_t *root_node;
};


} // namespace bvh


#endif
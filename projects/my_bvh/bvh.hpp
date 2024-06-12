#ifndef NEW_BVH_HPP
#define NEW_BVH_HPP

#include <vector>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

namespace horizon {

using namespace glm;

float safe_inverse(float x) {
    const float epsilon = std::numeric_limits<float>::epsilon();
    if (std::abs(x) <= epsilon) {
        if (x >= 0) return 1.f / epsilon;
        return -1.f / epsilon;
    }
    return 1.f / x;
}

const float infinity = std::numeric_limits<float>::max();

vec3 inverse_vec3(const vec3& v) {
    return vec3{ safe_inverse(v.x), safe_inverse(v.y), safe_inverse(v.z) };
}

struct ray_t {
    static ray_t new_ray(vec3 origin, vec3 direction) {
        ray_t ray;
        ray.origin = origin;
        ray.direction = direction;
        ray.inverse_direction = inverse_vec3(ray.direction);
        ray.tmin = 0;
        ray.tmax = infinity;
        return ray;
    }

    bool operator==(const ray_t& other) const {
        return origin == other.origin && direction == other.direction;
    }

    vec3 origin, direction, inverse_direction;
    float tmin, tmax;
};

struct aabb_intersection_t {
    operator bool() const {
        return tmin <= tmax;
    }
    float tmin;
    float tmax;
};

struct aabb_t {
    aabb_t() : _min(infinity), _max(-infinity) {}
    aabb_t(const vec3& min, const vec3& max) : _min(min), _max(max) {}

    aabb_t& extend(const aabb_t& other) {
        _min = min(_min, other._min);
        _max = max(_max, other._max);
        return *this;
    }

    aabb_t& extend(const vec3& point) {
        _min = min(_min, point);
        _max = max(_max, point);
        return *this;
    }

    aabb_intersection_t intersect(const ray_t& ray) const {
        vec3 tmin = (_min - ray.origin) * ray.inverse_direction;
        vec3 tmax = (_max - ray.origin) * ray.inverse_direction;

        const vec3 old_tmin = tmin;
        const vec3 old_tmax = tmax;

        tmin = min(old_tmin, old_tmax);
        tmax = max(old_tmin, old_tmax);

        float _tmin = max(tmin[0], max(tmin[1], max(tmin[2], ray.tmin)));
        float _tmax = min(tmax[0], min(tmax[1], min(tmax[2], ray.tmax)));

        return { _tmin, _tmax };
    }

    float area() const {
        vec3 extent = _max - _min;
        return extent.x * extent.y + extent.y * extent.z + extent.z * extent.x;
    }

    vec3 center() const {
        return (_min + _max) / 2.f;
    }

    vec3 _min, _max;
};

struct hit_blas_t {
    operator bool() {
        return primitive_id != static_cast<uint32_t>(-1);
    }

    uint32_t primitive_id = static_cast<uint32_t>(-1);
    float t;
    float u, v, w;
};

struct hit_tlas_t {
    operator bool() {
        return primitive_id != static_cast<uint32_t>(-1) && instance_id != static_cast<uint32_t>(-1);
    }

    uint32_t primitive_id = static_cast<uint32_t>(-1);
    uint32_t instance_id = static_cast<uint32_t>(-1);
    float t;
    float u, v, w;
};

struct node_t {
    vec3 min{ infinity };
    uint32_t primitive_count;
    vec3 max{ -infinity };
    uint32_t first_index;
};

struct bvh_t {
    bvh_t() = default;

    bvh_t(const aabb_t *aabbs, const vec3 *centers, size_t primitive_count) {
        primitive_indices.resize(primitive_count);
        for (uint32_t i = 0; i < primitive_count; i++) primitive_indices[i] = i;

        nodes.resize(2 * primitive_count - 1);
        nodes[0].primitive_count = primitive_count;
        nodes[0].first_index = 0;

        parent_node_ids.resize(2 * primitive_count - 1);
        parent_node_ids[0] = static_cast<uint32_t>(-1);

        uint32_t node_count = 1;
        update_node_bounds(0, aabbs, centers);
        subdivide_node(0, node_count, aabbs, centers);
        nodes.resize(node_count);
        parent_node_ids.resize(node_count);
    }

    void update_node_bounds(uint32_t node_id, const aabb_t *aabbs, const vec3 *centers) {
        node_t& node = nodes[node_id];
        aabb_t aabb{ node.min, node.max };
        for (uint32_t i = 0; i < node.primitive_count; i++) {
            uint32_t primitive_id = node.first_index + i;
            aabb.extend(aabbs[primitive_indices[primitive_id]]);
        }
        node.min = aabb._min;
        node.max = aabb._max;
    }

    std::pair<uint32_t, float> choose_axis_and_split_pos_basic(node_t& node, const aabb_t *aabbs, const vec3 *centers) {
        vec3 extent = node.max - node.min;
        uint32_t axis = 0;
        if (extent.y > extent.x) axis = 1;
        if (extent.z > extent[axis]) axis = 2;
        float split_pos = node.min[axis] + extent[axis] * 0.5f;
        return { axis, split_pos };
    }

    std::pair<uint32_t, float> choose_axis_and_split_pos_sah(node_t& node, const aabb_t *aabbs, const vec3 *centers) {
        auto evaluate_sah = [this](node_t& node, uint32_t axis, float split_pos, const aabb_t *aabbs, const vec3 *centers) -> float {
            aabb_t left_box{}, right_box{};
            uint32_t left_count = 0, right_count = 0;
            for (uint32_t i = 0; i < node.primitive_count; i++) {
                uint32_t primitive_id = node.first_index + i;
                if (centers[primitive_indices[primitive_id]][axis] < split_pos) {
                    left_count++;
                    left_box.extend(aabbs[primitive_indices[primitive_id]]);
                } else {
                    right_count++;
                    right_box.extend(aabbs[primitive_indices[primitive_id]]);
                }
            }
            float cost = left_count * left_box.area() + right_count * right_box.area();
            if (isnan(cost)) return infinity;
            return cost > 0 ? cost : infinity;
        };

        uint32_t best_axis = std::numeric_limits<uint32_t>::max();
        float best_split_pos = 0;
        float best_cost = infinity;

        for (uint32_t axis = 0; axis < 3; axis++) for (uint32_t i = 0; i < node.primitive_count; i++) {
            uint32_t primitive_id = node.first_index + i;
            float split_pos = centers[primitive_indices[primitive_id]][axis];
            float cost = evaluate_sah(node, axis, split_pos, aabbs, centers);
            if (cost < best_cost) {
                best_cost = cost;
                best_split_pos = split_pos;
                best_axis = axis;
            }
        }
        return { best_axis == std::numeric_limits<uint32_t>::max() ? 0 : best_axis, best_split_pos };
    }

    void subdivide_node(uint32_t node_id, uint32_t& node_count, const aabb_t *aabbs, const vec3 *centers) {
        node_t& node = nodes[node_id];
        if (node.primitive_count <= 2) return;


        // auto [axis, split_pos] = choose_axis_and_split_pos_basic(node, aabbs, centers);
        auto [axis, split_pos] = choose_axis_and_split_pos_sah(node, aabbs, centers);
        
        uint32_t i = node.first_index;  // first index
        uint32_t j = node.first_index + node.primitive_count - 1;  // last index

        while (i < j) {
            if (centers[primitive_indices[i]][axis] < split_pos) i++;
            // already in correct spot, so moves i front by 1
            else {
                // swaps current i with j, moves j back by 1
                std::swap(primitive_indices[i], primitive_indices[j]);  
                j--;
            }
        }
        uint32_t left_count = i - node.first_index;
        if (left_count == 0 || left_count == node.primitive_count) return;


        uint32_t left_node_id = node_count++;
        uint32_t right_node_id = node_count++;
        nodes[left_node_id].first_index = node.first_index;
        nodes[left_node_id].primitive_count = left_count;
        nodes[right_node_id].first_index = i;
        nodes[right_node_id].primitive_count = node.primitive_count - left_count;
        node.first_index = left_node_id;
        node.primitive_count = 0;

        parent_node_ids[left_node_id] = node_id;
        parent_node_ids[right_node_id] = node_id;


        update_node_bounds(left_node_id, aabbs, centers);
        update_node_bounds(right_node_id, aabbs, centers);

        subdivide_node(left_node_id, node_count, aabbs, centers);
        subdivide_node(right_node_id, node_count, aabbs, centers);
    }

    template <typename primitive_t>
    hit_blas_t stackless_traverse_blas(ray_t& ray, primitive_t *primitives) const {
        hit_blas_t hit{};
        
        auto get_near_child_id = [this](ray_t& ray, uint32_t node_id) -> uint32_t {
            const node_t& node = nodes[node_id];
            if (node.primitive_count != 0) return static_cast<uint32_t>(-1);
            return node.first_index + 1;
        };
        
        auto get_parent_id = [this](uint32_t node_id) -> uint32_t {
            return parent_node_ids[node_id];
        };

        enum traversal_state_t {
            e_from_parent,
            e_from_sibling,
            e_from_child,
        } state = traversal_state_t::e_from_parent;

        const node_t& current_node = nodes[0];
        if (current_node.primitive_count) {
            for (uint32_t i = 0; i < current_node.primitive_count; i++) {
                // hit.primitive_intersection_test_count++;
                const uint32_t primitive_id = primitive_indices[current_node.first_index + i];
                auto primitive_intersection = primitives[primitive_id].intersect(ray);
                if (primitive_intersection) {
                    ray.tmax = primitive_intersection.t;
                    hit.primitive_id = primitive_id;
                    hit.t = primitive_intersection.t;
                    hit.u = primitive_intersection.u;
                    hit.v = primitive_intersection.v;
                    hit.w = primitive_intersection.w;
                }
            }
            return hit;
        }

        uint32_t current = get_near_child_id(ray, 0);

        while (true) {
            switch (state) {
                case traversal_state_t::e_from_child:
                {
                    if (current == 0) return hit;
                    if (current == get_near_child_id(ray, get_parent_id(current))) {
                        const node_t& parent = nodes[get_parent_id(current)];
                        current = current == parent.first_index ? parent.first_index + 1 : parent.first_index;  
                        state = traversal_state_t::e_from_sibling;
                    } else {
                        current = get_parent_id(current);
                        state = traversal_state_t::e_from_child;
                    }
                }
                break;

                case traversal_state_t::e_from_sibling:
                {
                    // hit.node_intersection_test_count++;
                    const node_t& current_node = nodes[current];
                    aabb_t aabb{ current_node.min, current_node.max };
                    if (!aabb.intersect(ray)) {
                        current = get_parent_id(current);
                        state = traversal_state_t::e_from_child;
                    } else if (current_node.primitive_count) {
                        for (uint32_t i = 0; i < current_node.primitive_count; i++) {
                            // hit.primitive_intersection_test_count++;
                            uint32_t primitive_id = primitive_indices[current_node.first_index + i];
                            auto primitive_intersection = primitives[primitive_id].intersect(ray);
                            if (primitive_intersection) {
                                ray.tmax = primitive_intersection.t;
                                hit.primitive_id = primitive_id;
                                hit.t = primitive_intersection.t;
                                hit.u = primitive_intersection.u;
                                hit.v = primitive_intersection.v;
                                hit.w = primitive_intersection.w;
                            }
                        }
                        current = get_parent_id(current);
                        state = traversal_state_t::e_from_child;
                    } else {
                        current = get_near_child_id(ray, current);
                        state = traversal_state_t::e_from_parent;
                    }
                }
                break;

                case traversal_state_t::e_from_parent:
                {
                    if (current == static_cast<uint32_t>(-1)) return hit;
                    // hit.node_intersection_test_count++;
                    const node_t& current_node = nodes[current];
                    aabb_t aabb{ current_node.min, current_node.max };
                    if (!aabb.intersect(ray)) {
                        const node_t& parent = nodes[get_parent_id(current)];
                        current = current == parent.first_index ? parent.first_index + 1 : parent.first_index;  
                        state = traversal_state_t::e_from_sibling;
                    } else if (current_node.primitive_count) {
                        for (uint32_t i = 0; i < current_node.primitive_count; i++) {
                            // hit.primitive_intersection_test_count++;
                            uint32_t primitive_id = primitive_indices[current_node.first_index + i];
                            auto primitive_intersection = primitives[primitive_id].intersect(ray);
                            if (primitive_intersection) {
                                ray.tmax = primitive_intersection.t;
                                hit.primitive_id = primitive_id;
                                hit.t = primitive_intersection.t;
                                hit.u = primitive_intersection.u;
                                hit.v = primitive_intersection.v;
                                hit.w = primitive_intersection.w;
                            }
                        }
                        const node_t& parent = nodes[get_parent_id(current)];
                        current = current == parent.first_index ? parent.first_index + 1 : parent.first_index;  
                        state = traversal_state_t::e_from_sibling;
                    } else {
                        current = get_near_child_id(ray, current);
                        state = traversal_state_t::e_from_parent;
                    }
                }
                break;
            }
        }

        return hit;
    }

    template <typename primitive_t>
    hit_tlas_t stackless_traverse_tlas(ray_t& ray, primitive_t *primitives) const {
        hit_tlas_t hit{};
        
        auto get_near_child_id = [this](ray_t& ray, uint32_t node_id) -> uint32_t {
            const node_t& node = nodes[node_id];
            // if (node.primitive_count != 0) return static_cast<uint32_t>(-1);
            return node.first_index;
        };
        
        auto get_parent_id = [this](uint32_t node_id) -> uint32_t {
            return parent_node_ids[node_id];
        };

        enum traversal_state_t {
            e_from_parent,
            e_from_sibling,
            e_from_child,
        } state = traversal_state_t::e_from_parent;

        const node_t& current_node = nodes[0];
        if (current_node.primitive_count) {
            for (uint32_t i = 0; i < current_node.primitive_count; i++) {
                // hit.primitive_intersection_test_count++;
                const uint32_t primitive_id = primitive_indices[current_node.first_index + i];
                if (auto primitive_hit = primitives[primitive_id].intersect(ray)) {
                    ray.tmax = primitive_hit.t;
                    hit.instance_id = primitive_id;
                    hit.primitive_id = primitive_hit.primitive_id;
                    hit.t = primitive_hit.t;
                    hit.u = primitive_hit.u;
                    hit.v = primitive_hit.v;
                    hit.w = primitive_hit.w;
                }
            } 
            return hit;
        }

        uint32_t current = get_near_child_id(ray, 0);

        while (true) {
            switch (state) {
                case traversal_state_t::e_from_child:
                {
                    if (current == 0) return hit;
                    if (current == get_near_child_id(ray, get_parent_id(current))) {
                        const node_t& parent = nodes[get_parent_id(current)];
                        current = current == parent.first_index ? parent.first_index + 1 : parent.first_index;  
                        state = traversal_state_t::e_from_sibling;
                    } else {
                        current = get_parent_id(current);
                        state = traversal_state_t::e_from_child;
                    }
                }
                break;

                case traversal_state_t::e_from_sibling:
                {
                    // hit.node_intersection_test_count++;
                    const node_t& current_node = nodes[current];
                    aabb_t aabb{ current_node.min, current_node.max };
                    if (!aabb.intersect(ray)) {
                        current = get_parent_id(current);
                        state = traversal_state_t::e_from_child;
                    } else if (current_node.primitive_count) {
                        for (uint32_t i = 0; i < current_node.primitive_count; i++) {
                            const uint32_t primitive_id = primitive_indices[current_node.first_index + i];
                            auto primitive_hit = primitives[primitive_id].intersect(ray);
                            if (primitive_hit) {
                                ray.tmax = primitive_hit.t;
                                hit.instance_id = primitive_id;
                                hit.primitive_id = primitive_hit.primitive_id;
                                hit.t = primitive_hit.t;
                                hit.u = primitive_hit.u;
                                hit.v = primitive_hit.v;
                                hit.w = primitive_hit.w;
                            }
                        }
                        current = get_parent_id(current);
                        state = traversal_state_t::e_from_child;
                    } else {
                        current = get_near_child_id(ray, current);
                        state = traversal_state_t::e_from_parent;
                    }
                }
                break;

                case traversal_state_t::e_from_parent:
                {
                    // if (current == static_cast<uint32_t>(-1)) return hit;
                    // hit.node_intersection_test_count++;
                    const node_t& current_node = nodes[current];
                    aabb_t aabb{ current_node.min, current_node.max };
                    if (!aabb.intersect(ray)) {
                        const node_t& parent = nodes[get_parent_id(current)];
                        current = current == parent.first_index ? parent.first_index + 1 : parent.first_index;  
                        state = traversal_state_t::e_from_sibling;
                    } else if (current_node.primitive_count) {
                        for (uint32_t i = 0; i < current_node.primitive_count; i++) {
                            const uint32_t primitive_id = primitive_indices[current_node.first_index + i];
                            auto primitive_hit = primitives[primitive_id].intersect(ray);
                            if (primitive_hit) {
                                ray.tmax = primitive_hit.t;
                                hit.instance_id = primitive_id;
                                hit.primitive_id = primitive_hit.primitive_id;
                                hit.t = primitive_hit.t;
                                hit.u = primitive_hit.u;
                                hit.v = primitive_hit.v;
                                hit.w = primitive_hit.w;
                            }
                        }
                        const node_t& parent = nodes[get_parent_id(current)];
                        current = current == parent.first_index ? parent.first_index + 1 : parent.first_index;  
                        state = traversal_state_t::e_from_sibling;
                    } else {
                        current = get_near_child_id(ray, current);
                        state = traversal_state_t::e_from_parent;
                    }
                }
                break;
            }
        }

        return hit;
    }

    // template <typename primitive_t>
    // hit_tlas_t short_stack_tlas(ray_t& ray, primitive_t *primitives) const {
    //     hit_tlas_t hit{};

    //     const uint32_t stack_size = 8;
    //     uint32_t stack[stack_size];
    //     uint32_t stack_top = 0;

    //     auto push_stack = [&stack, &stack_top](uint32_t node_id) {
    //         stack[stack_top++] = node_id;
    //     };

    //     auto pop_stack = [&stack, &stack_top]() -> uint32_t {
    //         return stack[--stack_top];
    //     };

    //     auto get_near_child_id = [this](ray_t& ray, uint32_t node_id) -> uint32_t {
    //         const node_t& node = nodes[node_id];
    //         // if (node.primitive_count != 0) return static_cast<uint32_t>(-1);
    //         return node.first_index;
    //     };
        
    //     auto get_parent_id = [this](uint32_t node_id) -> uint32_t {
    //         return parent_node_ids[node_id];
    //     };

    //     bool using_stack = true;

    //     uint32_t current = 0;
    //     push_stack(0);

    //     enum traversal_state_t {
    //         e_from_parent,
    //         e_from_sibling,
    //         e_from_child,
    //     } state = traversal_state_t::e_from_parent;

    //     while (true) {
    //         if (using_stack) {
    //             if (!stack_top) return hit;
    //             current = pop_stack();
    //             const node_t& current_node = nodes[current];
    //             aabb_t current_node_aabb{ current_node.min, current_node.max };
    //             if (!current_node_aabb.intersect(ray)) continue;
    //             if (current_node.primitive_count) {
    //                 for (uint32_t i = 0; i < current_node.primitive_count; i++) {
    //                     const uint32_t primitive_id = primitive_indices[current_node.first_index + i];
    //                     auto primitive_hit = primitives[primitive_id].intersect(ray);
    //                     if (primitive_hit) {
    //                         ray.tmax = primitive_hit.t;
    //                         hit.instance_id = primitive_id;
    //                         hit.primitive_id = primitive_hit.primitive_id;
    //                         hit.t = primitive_hit.t;
    //                         hit.u = primitive_hit.u;
    //                         hit.v = primitive_hit.v;
    //                         hit.w = primitive_hit.w;
    //                     }
    //                 }
    //             } else {
    //                 push_stack(current_node.first_index + 0);
    //                 if (stack_top != stack_size) 
    //                     push_stack(current_node.first_index + 1);
    //                 else  {
    //                     current = current_node.first_index + 1;
    //                     using_stack = false;
    //                     state = traversal_state_t::e_from_parent;
    //                 }
    //             }
    //         } else {
    //             switch (state) {
                    
    //             }
    //         }
    //     }

    //     return hit;
    // } 

    template <typename primitive_t>
    hit_blas_t stack_traverse_blas(ray_t& ray, primitive_t *primitives) const {
        hit_blas_t hit{};
        
        uint32_t stack[32];
        uint32_t stack_top = 0;

        auto push_stack = [&stack, &stack_top](uint32_t node_id) {
            stack[stack_top++] = node_id;
        };

        auto pop_stack = [&stack, &stack_top]() -> uint32_t {
            return stack[--stack_top];
        };

        push_stack(0);

        while (stack_top) {
            const node_t& node = nodes[pop_stack()];

            aabb_t node_aabb{ node.min, node.max };
            if (!node_aabb.intersect(ray)) continue;
            if (node.primitive_count) {  // node is a leaf node
                for (uint32_t i = 0; i < node.primitive_count; i++) {
                    const uint32_t primitive_id = primitive_indices[node.first_index + i];
                    auto primitive_hit = primitives[primitive_id].intersect(ray);
                    if (primitive_hit) {
                        ray.tmax = primitive_hit.t;
                        hit.primitive_id = primitive_id;
                        hit.t = primitive_hit.t;
                        hit.u = primitive_hit.u;
                        hit.v = primitive_hit.v;
                        hit.w = primitive_hit.w;
                    }
                }
            } else {
                push_stack(node.first_index + 0);
                push_stack(node.first_index + 1);
            }
        }

        return hit;
    }


    template <typename primitive_t>
    hit_tlas_t stack_traverse_tlas(ray_t& ray, primitive_t *primitives) const {
        hit_tlas_t hit{};
        
        uint32_t stack[32];
        uint32_t stack_top = 0;

        auto push_stack = [&stack, &stack_top](uint32_t node_id) {
            stack[stack_top++] = node_id;
        };

        auto pop_stack = [&stack, &stack_top]() -> uint32_t {
            return stack[--stack_top];
        };

        push_stack(0);

        while (stack_top) {
            const node_t& node = nodes[pop_stack()];

            aabb_t node_aabb{ node.min, node.max };
            if (!node_aabb.intersect(ray)) continue;
            if (node.primitive_count) {  // node is a leaf node
                for (uint32_t i = 0; i < node.primitive_count; i++) {
                    const uint32_t primitive_id = primitive_indices[node.first_index + i];
                    auto primitive_hit = primitives[primitive_id].intersect(ray);
                    if (primitive_hit) {
                        ray.tmax = primitive_hit.t;
                        hit.instance_id = primitive_id;
                        hit.primitive_id = primitive_hit.primitive_id;
                        hit.t = primitive_hit.t;
                        hit.u = primitive_hit.u;
                        hit.v = primitive_hit.v;
                        hit.w = primitive_hit.w;
                    }
                }
            } else {
                push_stack(node.first_index + 0);
                push_stack(node.first_index + 1);
            }
        }

        return hit;
    }

    std::vector<node_t> nodes;
    std::vector<uint32_t> primitive_indices;
    std::vector<uint32_t> parent_node_ids;
};

template <typename primitive_t>
struct blas_instance_t {
    blas_instance_t() = default;
    blas_instance_t(bvh_t *bvh, primitive_t *primitives, const mat4& transform) : bvh(bvh), primitives(primitives) {
        auto& node = bvh->nodes[0];
        for (uint32_t i = 0; i < 8; i++) {
            vec3 pos = {
                i & 1 ? node.max.x : node.min.x,
                i & 2 ? node.max.y : node.min.y,
                i & 3 ? node.max.z : node.min.z,
            };
            pos = transform * vec4(pos, 1.f);
            aabb.extend(pos);
        }
        inverse_transform = inverse(transform);
    }
    
    hit_blas_t intersect(ray_t& ray) const {
        if (aabb.intersect(ray)) {
            ray_t backup_ray = ray;
            ray.origin = inverse_transform * vec4(ray.origin, 1.f);
            ray.direction = inverse_transform * vec4(ray.direction, 0.f);
            ray.inverse_direction = inverse_vec3(ray.direction);
            // hit = bvh.stack_traverse(ray, primitives);
            hit_blas_t hit = bvh->stack_traverse_blas(ray, primitives);
            backup_ray.tmax = ray.tmax;
            ray = backup_ray;

            return hit;
        }
        return hit_blas_t{};
    }
    bvh_t *bvh;
    primitive_t *primitives;
    aabb_t aabb;
    mat4 inverse_transform;
};  

} // namespace horizon

#endif
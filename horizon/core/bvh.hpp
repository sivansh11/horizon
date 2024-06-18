#ifndef CORE_BVH_HPP
#define CORE_BVH_HPP

#include "core.hpp"
#include "core/model.hpp"
#include "core/algebra.hpp"

#include <filesystem>

namespace core {

// const float infinity = std::numeric_limits<float>::max();

// struct triangle_t {
//     vec3 center() { return (v0.position + v1.position + v2.position) / 3.f; }
//     vertex_t v0{}, v1{}, v2{};
// };

// struct aabb_t {
//     aabb_t& grow(const vec3& point) {
//         min = glm::min(min, point);
//         max = glm::max(max, point);
//         return *this;
//     }

//     aabb_t& grow(const aabb_t& aabb) {
//         min = glm::min(min, aabb.min);
//         max = glm::max(max, aabb.max);
//         return *this;
//     }

//     aabb_t& grow(const triangle_t& triangle) {
//         grow(triangle.v0.position).grow(triangle.v1.position).grow(triangle.v2.position);
//         return *this;
//     }

//     float half_area() const {
//         vec3 e = max - min;
//         return e.x * e.y + e.y * e.z + e.z * e.x;
//     }

//     vec3 center() const {
//         return (max + min) / 2.f;
//     }

//     vec3 min{ infinity }, max{ -infinity };
// };

struct node_t {
    bool is_leaf() const { return primitive_count != 0; }

    vec3        min{ infinity };
    uint32_t    first_index = 0;  // first index to a primitive or a child node
    vec3        max{ -infinity };
    uint32_t    primitive_count = 0;
};

struct bvh_t {
    
    // TODO: find better values for these

    struct build_options_t {
        // for tlas primitive intersection cost should be much much higher
        const float primitive_intersection_cost = 1.1f;
        const float node_intersection_cost = 1.f;
        const uint32_t min_primitive_count = 1;
        const uint32_t max_primitive_count = std::numeric_limits<uint32_t>::max();
        const uint32_t sah_samples = 8;
        const bool add_node_intersection_cost_in_leaf_traversal = false;
    };

    static bvh_t construct(const aabb_t *p_aabbs, const vec3 *p_centers, uint32_t primitive_count, build_options_t build_options) {
        bvh_t bvh{};

        bvh.primitive_count = primitive_count;

        bvh.p_nodes = new node_t[2 * primitive_count - 1];
        bvh.p_primitive_indices = new uint32_t[primitive_count];
        bvh.p_parent_ids = new uint32_t[2 * primitive_count - 1];

        bvh.p_nodes[0] = node_t{};
        bvh.p_nodes[0].primitive_count = primitive_count;
        bvh.p_nodes[0].first_index = 0;

        bvh.p_parent_ids[0] = static_cast<uint32_t>(-1);

        for (uint32_t i = 0; i < primitive_count; i++) bvh.p_primitive_indices[i] = i;

        uint32_t node_count = 1;
        bvh.update_node_bounds(0, p_aabbs, p_centers);
        bvh.try_split_node(0, node_count, p_aabbs, p_centers, build_options);

        bvh.node_count = node_count;

        return bvh;
    }

    static void destroy(bvh_t *bvh) {
        assert(bvh);
        if (bvh->p_nodes)             delete[] bvh->p_nodes;
        if (bvh->p_primitive_indices) delete[] bvh->p_primitive_indices;
    }

    void update_node_bounds(uint32_t node_id, const aabb_t *p_aabbs, const vec3 *p_centers) {
        node_t& node = p_nodes[node_id];
        aabb_t aabb{ node.min, node.max };
        for (uint32_t i = 0; i < node.primitive_count; i++) {
            const uint32_t primitive_id = p_primitive_indices[node.first_index + i];
            aabb.grow(p_aabbs[primitive_id]);
        }
        node.min = aabb.min;
        node.max = aabb.max;
    }

    struct debug_data_t {
        aabb_t left_aabb, right_aabb;
        uint32_t left_count, right_count;
    };

    struct split_data_t {
        uint32_t    axis = 0;
        float       position = 0;
        debug_data_t debug_data;
    };

    float leaf_node_traversal_cost(uint32_t primitive_count, build_options_t build_options) const {
        if (build_options.add_node_intersection_cost_in_leaf_traversal)
            return (build_options.primitive_intersection_cost * primitive_count) + build_options.node_intersection_cost;
        else 
            return (build_options.primitive_intersection_cost * primitive_count);
    }

    float get_split_cost(uint32_t node_id, split_data_t split_data, const aabb_t *p_aabbs, const vec3 *p_centers, build_options_t build_options, debug_data_t& debug_data) {
        const node_t& node = p_nodes[node_id];
        const aabb_t aabb{ node.min, node.max };

        aabb_t left_aabb{}, right_aabb{};
        uint32_t left_count = 0, right_count = 0;
        for (uint32_t i = 0; i < node.primitive_count; i++) {
            const uint32_t primitive_id = p_primitive_indices[node.first_index + i];
            if (p_centers[primitive_id][split_data.axis] < split_data.position) {
                left_aabb.grow(p_aabbs[primitive_id]);
                left_count++;
            } else {
                right_aabb.grow(p_aabbs[primitive_id]);
                right_count++;
            }
        }
        float left_cost;
        float right_cost;
        // if split is unsuccessful (ie, cannot partition), split cost is infinity
        if (build_options.add_node_intersection_cost_in_leaf_traversal) {
            left_cost  = left_count  ? ((leaf_node_traversal_cost(left_count, build_options)  - build_options.node_intersection_cost) * (left_aabb.half_area()  / aabb.half_area())) + build_options.node_intersection_cost : infinity;
            right_cost = right_count ? ((leaf_node_traversal_cost(right_count, build_options) - build_options.node_intersection_cost) * (right_aabb.half_area() / aabb.half_area())) + build_options.node_intersection_cost : infinity;
        } else {
            left_cost  = left_count  ? leaf_node_traversal_cost(left_count, build_options)  * (left_aabb.half_area()  / aabb.half_area()) : infinity;
            right_cost = right_count ? leaf_node_traversal_cost(right_count, build_options) * (right_aabb.half_area() / aabb.half_area()) : infinity;
        }
        if (left_cost == infinity || right_cost == infinity) {
            return infinity;
        }
        debug_data.left_aabb = left_aabb;
        debug_data.right_aabb = right_aabb;
        debug_data.left_count = left_count;
        debug_data.right_count = right_count;
        return left_cost + right_cost + build_options.node_intersection_cost;
    }

    std::pair<split_data_t, float> find_best_split(uint32_t node_id, const aabb_t *p_aabbs, const vec3 *p_centers, build_options_t build_options) {
        node_t& node = p_nodes[node_id];
        
        aabb_t split_bounds{};
        for (uint32_t i = 0; i < node.primitive_count; i++) {
            const uint32_t primitive_id = p_primitive_indices[node.first_index + i];
            split_bounds.grow(p_centers[primitive_id]);
        }

        split_data_t best_split{};
        float best_cost = infinity;

        constexpr bool use_uniform_sampling = true;

        for (uint32_t axis = 0; axis < 3; axis++) {
            if (use_uniform_sampling) {
                const float scale = (split_bounds.max[axis] - split_bounds.min[axis]) / float(build_options.sah_samples + 1);
                if (scale == 0.f) continue;
                for (uint32_t i = 0; i <= build_options.sah_samples; i++) {
                    split_data_t candidate_split{};
                    candidate_split.axis = axis;
                    candidate_split.position = split_bounds.min[axis] + (i * scale);

                    debug_data_t debug_data;
                    float cost = get_split_cost(node_id, candidate_split, p_aabbs, p_centers, build_options, debug_data);
                    if (cost < best_cost) {
                        best_cost = cost;
                        best_split = candidate_split;
                        best_split.debug_data = debug_data;
                    }
                } 
            } else {
                for (uint32_t i = 0; i < node.primitive_count; i++) {
                    const uint32_t primitive_id = p_primitive_indices[node.first_index + i];
                    split_data_t candidate_split;
                    candidate_split.axis = axis;
                    candidate_split.position = p_centers[primitive_id][axis];
                    
                    debug_data_t debug_data;
                    float cost = get_split_cost(node_id, candidate_split, p_aabbs, p_centers, build_options, debug_data);
                    if (cost < best_cost) {
                        best_cost = cost;
                        best_split = candidate_split;
                        best_split.debug_data = debug_data;
                    }
                }
            }
        }
        return { best_split, best_cost };
    }

    std::pair<bool, split_data_t> should_split_node(uint32_t node_id, const aabb_t *p_aabbs, const vec3 *p_centers, build_options_t build_options) {
        node_t& node = p_nodes[node_id];
        aabb_t aabb{ node.min, node.max };

        float no_split_cost = leaf_node_traversal_cost(node.primitive_count, build_options); 
        
        auto [split_data, split_cost] = find_best_split(node_id, p_aabbs, p_centers, build_options);

        if (no_split_cost < split_cost) {
            return { false, {} };  // shouldnt split
        } else {
            return { true, split_data };
        }
    }

    void split_node(uint32_t node_id, uint32_t& node_count, split_data_t split_data, const aabb_t *p_aabbs, const vec3 *p_centers, build_options_t build_options) {
        node_t& node = p_nodes[node_id];

        // std::vector<uint32_t> primitive_indices{};
        // primitive_indices.resize(node.primitive_count);

        // uint32_t start = 0;
        // uint32_t end = node.primitive_count - 1;

        // for (uint32_t i = 0; i < node.primitive_count; i++) {
        //     const uint32_t primitive_id = p_primitive_indices[node.first_index + i];
        //     primitive_indices[i] = primitive_id;
        // }

        // uint32_t left_count = 0;
        // uint32_t right_count = 0;
        // for (uint32_t i = 0; i < node.primitive_count; i++) {
        //     const uint32_t primitive_id = p_primitive_indices[node.first_index + i];
        //     if (p_centers[primitive_id][split_data.axis] < split_data.position) {
        //         primitive_indices[start++] = primitive_id;
        //         left_count++;
        //     } else {
        //         primitive_indices[end--] = primitive_id;
        //         right_count++;
        //     }
        // }

        // for (uint32_t i = 0; i < node.primitive_count; i++) {
        //     p_primitive_indices[node.first_index + i] = primitive_indices[i];
        // }

        uint32_t left  = node.first_index;
        uint32_t right = left + node.primitive_count - 1;

        while (left <= right) {
            if (p_centers[p_primitive_indices[left]][split_data.axis] < split_data.position) {
                left++;
            } else {
                std::swap(p_primitive_indices[left], p_primitive_indices[right]);
                right--;
            }
        }
        
        uint32_t left_count = left - node.first_index;
        
        if (left_count == 0 || left_count == node.primitive_count) {
            return;
        }

        uint32_t left_node_id = node_count++;
        uint32_t right_node_id = node_count++;

        p_nodes[left_node_id] = node_t{};
        p_nodes[left_node_id].first_index = node.first_index;
        p_nodes[left_node_id].primitive_count = left_count;

        p_nodes[right_node_id] = node_t{};
        // p_nodes[right_node_id].first_index = node.first_index + left_count;
        // p_nodes[right_node_id].primitive_count = node.primitive_count - left_count;
        p_nodes[right_node_id].first_index = left;
        p_nodes[right_node_id].primitive_count = node.primitive_count - left_count;

        
        p_parent_ids[right_node_id] = node_id;
        p_parent_ids[left_node_id] = node_id;

        node.first_index = left_node_id;
        node.primitive_count = 0;

        update_node_bounds(left_node_id, p_aabbs, p_centers);
        update_node_bounds(right_node_id, p_aabbs, p_centers);

        try_split_node(left_node_id, node_count, p_aabbs, p_centers, build_options);
        try_split_node(right_node_id, node_count, p_aabbs, p_centers, build_options);
    }

    void force_split_node(uint32_t node_id, uint32_t& node_count, const aabb_t *p_aabbs, const vec3 *p_centers, build_options_t build_options) {
        auto [split_data, split_cost] = find_best_split(node_id, p_aabbs, p_centers, build_options);
        split_node(node_id, node_count, split_data, p_aabbs, p_centers, build_options);
    }

    void try_split_node(uint32_t node_id, uint32_t& node_count, const aabb_t *p_aabbs, const vec3 *p_centers, build_options_t build_options) {
        node_t& node = p_nodes[node_id];

        // check if tri count is less than what I want (force not split)
        // check if tri count is greater than what I want to be the max (force split)
        // check if split cost is greater than non split cost
        if (node.primitive_count <= build_options.min_primitive_count) return;
        if (node.primitive_count > build_options.max_primitive_count) {
            force_split_node(node_id, node_count, p_aabbs, p_centers, build_options);
            return;
        }
        auto [should_split, split_data] = should_split_node(node_id, p_aabbs, p_centers, build_options);
        if (!should_split) return;
        split_node(node_id, node_count, split_data, p_aabbs, p_centers, build_options);
    }

    // void old_split_node(uint32_t node_id, uint32_t& node_count, const aabb_t *p_aabbs, const vec3 *p_centers) {
    //     node_t& node = p_nodes[node_id];
    //     if (node.primitive_count < min_primitive_count) return;

    //     force_split_node(node_id, node_count, p_aabbs, p_centers);
    // }

    uint32_t depth(uint32_t node_id) const {
        node_t& node = p_nodes[node_id];
        return node.is_leaf() ? 1 : 1 + glm::max(depth(node.first_index), depth(node.first_index + 1));
    }

    uint32_t depth() const {
        return depth(0);
    }

    float node_traversal_cost(uint32_t node_id, build_options_t build_options) const {
        node_t& node = p_nodes[node_id];

        if (node.is_leaf()) {
            return leaf_node_traversal_cost(node.primitive_count, build_options);
        }

        aabb_t aabb{ node.min, node.max };

        node_t& left_node = p_nodes[node.first_index + 0];
        node_t& right_node = p_nodes[node.first_index + 1];

        aabb_t left_aabb{ left_node.min, left_node.max };
        aabb_t right_aabb{ right_node.min, right_node.max };

        const float left_hit_probablity = left_aabb.half_area()  / aabb.half_area();
        const float right_hit_probablity = right_aabb.half_area()  / aabb.half_area();

        float cost = (left_hit_probablity * node_traversal_cost(node.first_index + 0, build_options)) + (right_hit_probablity * node_traversal_cost(node.first_index + 1, build_options)) + build_options.node_intersection_cost;
        return cost;
    }

    float node_traversal_cost(build_options_t build_options) const {
        return node_traversal_cost(0, build_options);
    }

    void to_disk(std::filesystem::path path) {
        binary_writer_t binary_writer{ path };
        binary_writer.write(node_count);
        for (uint32_t i = 0; i < node_count; i++) {
            binary_writer.write(p_nodes[i]);
        }
        binary_writer.write(primitive_count);
        for (uint32_t i = 0; i < primitive_count; i++) {
            binary_writer.write(p_primitive_indices[i]);
        }
        for (uint32_t i = 0; i < node_count; i++) {
            binary_writer.write(p_parent_ids[i]);
        }
    }

    static bvh_t load(std::filesystem::path path) {
        bvh_t bvh{};

        binary_reader_t binary_reader{ path };
        binary_reader.read(bvh.node_count);
        bvh.p_nodes = new node_t[bvh.node_count];
        for (uint32_t i = 0; i < bvh.node_count; i++) {
            binary_reader.read(bvh.p_nodes[i]);
        }
        binary_reader.read(bvh.primitive_count);
        bvh.p_primitive_indices = new uint32_t[bvh.primitive_count];
        for (uint32_t i = 0; i < bvh.primitive_count; i++) {
            binary_reader.read(bvh.p_primitive_indices[i]);
        }
        bvh.p_parent_ids = new uint32_t[bvh.node_count];
        for (uint32_t i = 0; i < bvh.node_count; i++) {
            binary_reader.read(bvh.p_parent_ids[i]);
        }

        return bvh;
    }

    node_t *p_nodes{ nullptr };
    uint32_t *p_primitive_indices{ nullptr };
    uint32_t node_count;
    uint32_t primitive_count;
    uint32_t *p_parent_ids{ nullptr };
};

} // namespace core

#endif
#ifndef BVH_BUILDER_HPP
#define BVH_BUILDER_HPP

#include "math.hpp"
#include "aabb.hpp"

#include <vector>
#include <memory>
#include <span>

namespace core {
    
namespace bvh {

static const uint32_t invalid_index = std::numeric_limits<uint32_t>::max();

#define FIRST_INDEX_BITS_SIZE 28

// size(node_t) = (3+3+1+1) * 4
struct node_t {
    aabb_t          aabb{};
    uint32_t        is_leaf      : 1;
    uint32_t        primitive_count : 31;
    union as_t {
        struct leaf_t {
            uint32_t first_primitive_index  : FIRST_INDEX_BITS_SIZE;
            uint32_t dummy                  : 32 - FIRST_INDEX_BITS_SIZE;
        } leaf;
        struct internal_t {
            uint32_t first_child_index  : FIRST_INDEX_BITS_SIZE;
            uint32_t children_count     : 32 - FIRST_INDEX_BITS_SIZE;
        } internal;
    } as;

    bool operator==(const node_t& other) const {
        return aabb == other.aabb && is_leaf == other.is_leaf && primitive_count == other.primitive_count && as.leaf.first_primitive_index == other.as.leaf.first_primitive_index;
    }
};

static_assert(sizeof(node_t) == 32);

struct bvh_t {
    std::vector<node_t> nodes;
    std::vector<uint32_t> primitive_indices;
};

enum class object_split_search_type_t {
    e_longest_axis_division,
    e_full_sweep_sah,
    e_uniform_sah,
    e_binned_sah,
};

struct options_t {
    uint32_t                    o_min_primitive_count         = 1;  // TODO: try 0
    uint32_t                    o_max_primitive_count         = std::numeric_limits<uint32_t>::max();
    object_split_search_type_t  o_object_split_search_type    = object_split_search_type_t::e_binned_sah;
    float                       o_primitive_intersection_cost = 1.1f;
    float                       o_node_intersection_cost      = 1.f;
    uint32_t                    o_samples                     = 8;
};

bvh_t build_bvh2(std::span<aabb_t> aabbs, std::span<vec3> centers, uint32_t primitive_count, const options_t& options);

float cost_of_node(bvh_t& bvh, const options_t& options);

bvh_t convert_bvh2_to_bvh4(const bvh_t& bvh2);

} // namespace bvh

} // namespace core

#endif
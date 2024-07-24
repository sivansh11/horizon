#include "bvh.hpp"

#include "core.hpp"

#include <deque>

namespace core {

namespace bvh {

#define primitive_index bvh.primitive_indices[node.as.leaf.first_primitive_index + i]

node_t new_node(uint32_t first_primitive_index, uint32_t primitive_count, uint32_t parent_index) {
    node_t node{};
    
    horizon_assert(!node.aabb.is_valid(), "sanity check failed, aabb should be invalid/default state");

    node.is_leaf = true;
    node.as.leaf.first_primitive_index = first_primitive_index;
    node.primitive_count = primitive_count;

    return node;
}

void update_node_bounds(bvh_t& bvh, uint32_t node_index, std::span<aabb_t> aabbs) {
    node_t& node = bvh.nodes[node_index];
    if (node.is_leaf) {
        for (uint32_t i = 0; i < node.primitive_count; i++) {
            node.aabb.grow(aabbs[primitive_index]);
        }
    } else {
        node.aabb = {};
        for (auto& child : std::span(bvh.nodes.begin() + node.as.internal.first_child_index, node.as.internal.children_count)) {
            node.aabb.grow(child.aabb);
        }
    }
}

struct split_t {
    uint32_t    axis        = 4;
    float       position    = infinity;
};

uint32_t partition_node_indices(bvh_t& bvh, uint32_t node_index, split_t split, std::span<vec3> centers) {
    node_t& node = bvh.nodes[node_index];
    horizon_assert(node.is_leaf, "sanity check failed, only leaf node may be partitioned");

    auto middle = std::partition(
        bvh.primitive_indices.begin() + node.as.leaf.first_primitive_index,
        bvh.primitive_indices.begin() + node.as.leaf.first_primitive_index + node.primitive_count,
        [&](uint32_t index) {
            return centers[index][split.axis] < split.position;
        }
    );

    return std::distance(bvh.primitive_indices.begin(), middle);
}

void try_split_node(bvh_t& bvh, uint32_t node_index, std::span<aabb_t> aabbs, std::span<vec3> centers, const options_t& options);

bool split_node(bvh_t& bvh, uint32_t node_index, split_t split, std::span<aabb_t> aabbs, std::span<vec3> centers, const options_t& options) {
    if (split.axis == 4) return false;  // failed split

    node_t& node = bvh.nodes[node_index];
    horizon_assert(node.is_leaf, "sanity check");

    uint32_t right_first_primitive_index = partition_node_indices(bvh, node_index, split, centers);

    if (node.as.leaf.first_primitive_index == right_first_primitive_index || node.as.leaf.first_primitive_index + node.primitive_count == right_first_primitive_index) {
        return false;   // failed split
    }

    node_t left  = new_node(node.as.leaf.first_primitive_index, right_first_primitive_index - node.as.leaf.first_primitive_index, node_index);
    node_t right = new_node(right_first_primitive_index, node.as.leaf.first_primitive_index + node.primitive_count - right_first_primitive_index, node_index);

    node.is_leaf = false;
    check(core::number_of_bits_required_for_number(bvh.nodes.size()) <= FIRST_INDEX_BITS_SIZE, "number does not fit in to {} bits", FIRST_INDEX_BITS_SIZE); 
    node.as.internal.first_child_index = bvh.nodes.size();
    node.as.internal.children_count = 2;

    uint32_t left_node_index = bvh.nodes.size();
    bvh.nodes.push_back(left);

    uint32_t right_node_index = bvh.nodes.size();
    bvh.nodes.push_back(right);

    update_node_bounds(bvh, left_node_index , aabbs);
    update_node_bounds(bvh, right_node_index, aabbs);

    try_split_node(bvh, left_node_index , aabbs, centers, options);
    try_split_node(bvh, right_node_index, aabbs, centers, options);

    return true;
}

struct bin_t {
    aabb_t aabb{};
    uint32_t primitive_count = 0;
};

float greedy_cost_of_node(uint32_t left_count, uint32_t right_count, float left_aabb_area, float right_aabb_area, float parent_aabb_area, const options_t& options) {
    return options.o_node_intersection_cost + ((left_aabb_area * options.o_primitive_intersection_cost * left_count + right_aabb_area * options.o_primitive_intersection_cost * right_count) / parent_aabb_area);
}

float cost_of_node(bvh_t& bvh, uint32_t node_index, const options_t& options) {
    node_t& node = bvh.nodes[node_index];
    if (node.is_leaf) {
        return options.o_primitive_intersection_cost * node.primitive_count;
    } else {
        float cost = 0;
        for (uint32_t i = 0; i < node.as.internal.children_count; i++) {
            node_t& child = bvh.nodes[node.as.internal.first_child_index + i];
            cost += child.aabb.area() * cost_of_node(bvh, node.as.internal.first_child_index + i, options);
        }
        cost = options.o_node_intersection_cost + (cost / node.aabb.area());
        return cost;
    }
}

float cost_of_node(bvh_t& bvh, const options_t& options) {
    return cost_of_node(bvh, 0, options);    
}

std::pair<float, split_t> find_best_split(bvh_t& bvh, uint32_t node_index, std::span<aabb_t> aabbs, std::span<vec3> centers, const options_t& options) {
    node_t& node = bvh.nodes[node_index];
    horizon_assert(node.is_leaf, "sanity check");

    split_t best_split{};
    float best_cost = infinity;

    if (options.o_object_split_search_type == object_split_search_type_t::e_binned_sah) {
        aabb_t split_bounds{};
        for (uint32_t i = 0; i < node.primitive_count; i++) {
            split_bounds.grow(centers[primitive_index]);
        }

        for (uint32_t axis = 0; axis < 3; axis++) {
            if (split_bounds.max[axis] == split_bounds.min[axis]) continue;

            // bin_t *p_bins = reinterpret_cast<bin_t *>(alloca(sizeof(bin_t) * options.o_samples));
            // std::span<bin_t> bins{ p_bins, options.o_samples };
            bin_t bins[options.o_samples];
            for(auto& bin : bins) {
                bin = {};
            }

            float scale = static_cast<float>(options.o_samples) / (split_bounds.max[axis] - split_bounds.min[axis]);
            for (uint32_t i = 0; i < node.primitive_count; i++) {
                uint32_t bin_id = min(options.o_samples - 1, static_cast<uint32_t>((centers[primitive_index][axis] - split_bounds.min[axis]) * scale));
                bins[bin_id].primitive_count++;
                bins[bin_id].aabb.grow(aabbs[primitive_index]);
            }

            float left_area[options.o_samples - 1], right_area[options.o_samples - 1];
            uint32_t left_count[options.o_samples - 1], right_count[options.o_samples - 1];
            aabb_t left_aabb{}, right_aabb{};
            uint32_t left_sum = 0, right_sum = 0;
            for (uint32_t i = 0; i < options.o_samples - 1; i++) {
                left_sum += bins[i].primitive_count;
                left_count[i] = left_sum;
                left_aabb.grow(bins[i].aabb);
                left_area[i] = left_aabb.area();

                right_sum += bins[options.o_samples - i - 1].primitive_count;
                right_count[options.o_samples - i - 2] = right_sum;
                right_aabb.grow(bins[options.o_samples - i - 1].aabb);
                right_area[options.o_samples - i - 2] = right_aabb.area();
            }

            scale = (split_bounds.max[axis] - split_bounds.min[axis]) / static_cast<float>(options.o_samples);
            for (uint32_t i = 0; i < options.o_samples - 1; i++) {
                float cost = greedy_cost_of_node(left_count[i], right_count[i], left_area[i], right_area[i], node.aabb.area(), options);
                if (cost < best_cost) {
                    best_cost = cost;
                    best_split = { .axis = axis, .position = split_bounds.min[axis] + scale * (i + 1) };
                }
            } 
        }
    } else {
        throw std::runtime_error("not implemented");
    }

    return { best_cost, best_split };
}

void try_split_node(bvh_t& bvh, uint32_t node_index, std::span<aabb_t> aabbs, std::span<vec3> centers, const options_t& options) {
    node_t& node = bvh.nodes[node_index];
    horizon_assert(node.is_leaf, "sanity check");

    if (node.primitive_count <= options.o_min_primitive_count) return;

    auto [split_cost, split] = find_best_split(bvh, node_index, aabbs, centers, options);

    if (node.primitive_count > options.o_max_primitive_count) {
        bool did_split = split_node(bvh, node_index, split, aabbs, centers, options);
        // if (!did_split) log_warn("tried splitting, but failed");  // TODO: maybe handle this with some fallback split ?
    } else {
        float no_split_cost = cost_of_node(bvh, node_index, options);
        if (split_cost < no_split_cost) {
            bool did_split = split_node(bvh, node_index, split, aabbs, centers, options);
            // if (!did_split) log_warn("tried splitting, but failed");
        }
    }
}

bvh_t build_bvh2(std::span<aabb_t> aabbs, std::span<vec3> centers, uint32_t primitive_count, const options_t& options) {
    bvh_t bvh{};

    bvh.primitive_indices.reserve(primitive_count);
    for (uint32_t i = 0; i < primitive_count; i++) bvh.primitive_indices.push_back(i);

    node_t root = new_node(0, primitive_count, invalid_index);
    bvh.nodes.push_back(root);

    update_node_bounds(bvh, 0, aabbs);
    try_split_node(bvh, 0, aabbs, centers, options);

    return bvh;
}

bvh_t convert_bvh2_to_bvh4(const bvh_t& bvh2) {
    bvh_t bvh4{ .primitive_indices = bvh2.primitive_indices };

    std::vector<uint32_t> stack{ 0 };

    node_t bvh4_root = bvh2.nodes[0];
    bvh4.nodes.push_back(bvh4_root);
    uint32_t bvh4_nodes_used = 1;

    while (stack.size()) {
        node_t& parent = bvh4.nodes[stack.back()];
        if (parent.is_leaf) {
            stack.pop_back();
            continue;
        }

        const node_t& left  = bvh2.nodes[parent.as.internal.first_child_index + 0]; 
        const node_t& right = bvh2.nodes[parent.as.internal.first_child_index + 1]; 

        uint32_t children_count = 0;
        if (left.is_leaf) {
            bvh4.nodes.push_back(left);
            children_count++;
        } else {
            bvh4.nodes.push_back(bvh2.nodes[left.as.internal.first_child_index + 0]);
            children_count++;

            bvh4.nodes.push_back(bvh2.nodes[left.as.internal.first_child_index + 1]);
            children_count++;
        }
        if (right.is_leaf) {
            bvh4.nodes.push_back(right);
            children_count++;
        } else {
            bvh4.nodes.push_back(bvh2.nodes[right.as.internal.first_child_index + 0]);
            children_count++;

            bvh4.nodes.push_back(bvh2.nodes[right.as.internal.first_child_index + 1]);
            children_count++;
        }

        {
            // regetting parent as the original parent refrence may be invalid since a vector is a growing array
            node_t& parent = bvh4.nodes[stack.back()]; stack.pop_back();
            parent.as.internal.children_count = children_count;
            parent.as.internal.first_child_index = bvh4_nodes_used;
            bvh4_nodes_used += children_count;
        }


        for (uint32_t i = 1; i <= children_count; i++) {
            stack.push_back(bvh4_nodes_used - i);
        }
    }

    for (int32_t node_index = bvh4.nodes.size() - 1; node_index >= 0; node_index--) {
        node_t& node = bvh4.nodes[node_index];
        if (node.is_leaf) continue;
        node.aabb = {};
        for (uint32_t child_itr = 0; child_itr < node.as.internal.children_count; child_itr++) {
            node.aabb.grow(bvh4.nodes[node.as.internal.first_child_index + child_itr].aabb);
        }
    }

    return bvh4;
}

} // namespace bvh

} // namespace core

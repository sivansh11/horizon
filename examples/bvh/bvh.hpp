#ifndef BVH_HPP
#define BVH_HPP

#include <algorithm>
#include <alloca.h>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <span>
#include <vector>

#include "horizon/core/aabb.hpp"
#include "horizon/core/core.hpp"
#include "horizon/core/logger.hpp"
#include "horizon/core/math.hpp"

using namespace core;

namespace bvh {

static constexpr uint32_t invalid_index = std::numeric_limits<uint32_t>::max();

#define FIRST_INDEX_BITS_SIZE 28

struct node_t {
  aabb_t aabb{};
  uint32_t is_leaf : 1;
  uint32_t refrence_count : 31;
  union as_t {
    struct leaf_t {
      uint32_t first_reference_index : FIRST_INDEX_BITS_SIZE;
      uint32_t dummy : 32 - FIRST_INDEX_BITS_SIZE;
    } leaf;
    struct internal_t {
      uint32_t first_child_index : FIRST_INDEX_BITS_SIZE;
      uint32_t children_count : 32 - FIRST_INDEX_BITS_SIZE;
    } internal;
  } as;

  bool operator==(const node_t &other) const {
    return std::memcmp(this, &other, sizeof(node_t));
  }
};
static_assert(sizeof(node_t) == 32, "sizeof(core::bvh::node_t) != 32");

struct refrence_t {
  aabb_t aabb{};
  uint32_t primitive_index = invalid_index;
};

struct bvh_t {
  std::vector<uint32_t> parents;
  std::vector<node_t> nodes;
  std::vector<refrence_t> refrences;
};

struct options_t {
  float primitive_intersection_cost = 1.1f;
  float node_intersection_cost = 1.f;
  uint32_t object_samples = 8;
  uint32_t spatial_samples = 8;
};

struct split_t {
  uint32_t axis = 3; // 3 is invalid axis
  float position = infinity;
  float cost = infinity;
  aabb_t left_aabb{}, right_aabb{};
};

struct object_bin_t {
  aabb_t aabb{};
  uint32_t primitive_count = 0;
};

struct spatial_bin_t {
  aabb_t aabb{};
  uint32_t entries = 0;
  uint32_t exits = 0;
};

inline void update_node_bounds(bvh_t &bvh, uint32_t node_index) {
  node_t &node = bvh.nodes[node_index];
  horizon_assert(node.is_leaf,
                 "called update_node_bounds on node which is not a leaf");

  for (auto &refrence :
       std::span(bvh.refrences.begin() + node.as.leaf.first_reference_index,
                 bvh.refrences.begin() + node.as.leaf.first_reference_index +
                     node.refrence_count)) {
    node.aabb.grow(refrence.aabb);
  }
}

inline float greedy_cost_of_node(uint32_t left_count, uint32_t right_count,
                                 float left_aabb_area, float right_aabb_area,
                                 float parent_aabb_area, const options_t &o) {
  return o.node_intersection_cost +
         ((left_aabb_area * o.primitive_intersection_cost * left_count +
           right_aabb_area * o.primitive_intersection_cost * right_count) /
          parent_aabb_area);
}

inline split_t find_best_spatial_split(bvh_t &bvh, uint32_t node_index,
                                       const options_t &o) {
  node_t &node = bvh.nodes[node_index];
  split_t best_split{};

  aabb_t split_bounds{};
  for (auto &refrence :
       std::span(bvh.refrences.begin() + node.as.leaf.first_reference_index,
                 bvh.refrences.begin() + node.as.leaf.first_reference_index +
                     node.refrence_count)) {
    split_bounds.grow(refrence.aabb.center());
  }

  for (uint32_t axis = 0; axis < 3; axis++) {
    if (split_bounds.max[axis] == split_bounds.min[axis])
      continue;

    spatial_bin_t *p_bins = reinterpret_cast<spatial_bin_t *>(
        alloca(sizeof(spatial_bin_t) * o.spatial_samples));
    std::span bins{p_bins, o.spatial_samples};

    for (auto &bin : bins)
      bin = spatial_bin_t{.aabb = {.min = {0, 0, 0}, .max = {0, 0, 0}}};

    float width =
        (split_bounds.max[axis] - split_bounds.min[axis]) / o.spatial_samples;
    for (auto &refrence :
         std::span(bvh.refrences.begin() + node.as.leaf.first_reference_index,
                   bvh.refrences.begin() + node.as.leaf.first_reference_index +
                       node.refrence_count)) {

      // range is min->max
      // width of subrange is (max - min) / num_subranges

      uint32_t min_index =
          uint32_t((refrence.aabb.min[axis] - split_bounds.min[axis]) / width);
      uint32_t max_index = std::min(
          uint32_t((refrence.aabb.max[axis] - split_bounds.min[axis]) / width),
          o.spatial_samples - 1);
      bins[min_index].entries++;
      bins[max_index].exits++;
      for (uint32_t i = min_index; i <= max_index; i++) {
        // some how clip the aabb
        aabb_t refrence_aabb = refrence.aabb;
        refrence_aabb.min[axis] = std::max(refrence_aabb.min[axis], width * i);
        refrence_aabb.min[axis] =
            std::min(refrence_aabb.min[axis], width * (i + 1));
        refrence_aabb.max[axis] = std::max(refrence_aabb.max[axis], width * i);
        refrence_aabb.max[axis] =
            std::min(refrence_aabb.max[axis], width * (i + 1));

        bins[i].aabb.grow(refrence_aabb);
      }

      for (uint32_t i = 0; i < o.spatial_samples - 1; i++) {
        aabb_t left_aabb{}, right_aabb{};
        uint32_t left_count = 0, right_count = 0;
        for (uint32_t j = 0; j <= i; j++) {
          left_count += bins[j].entries;
          left_aabb.grow(bins[j].aabb);
        }
        for (uint32_t j = i + 1; j < o.spatial_samples; i++) {
          right_count += bins[j].exits;
          right_aabb.grow(bins[j].aabb);
        }
        float cost =
            greedy_cost_of_node(left_count, right_count, left_aabb.area(),
                                right_aabb.area(), node.aabb.area(), o);

        if (cost < best_split.cost) {
          best_split.cost = cost;
          best_split.axis = axis;
          best_split.position = split_bounds.min[axis] + width * (i + 1);
          best_split.left_aabb = left_aabb;
          best_split.right_aabb = right_aabb;
        }
      }
    }
  }
  return best_split;
}

inline split_t find_best_object_split(bvh_t &bvh, uint32_t node_index,
                                      const options_t &o) {
  node_t &node = bvh.nodes[node_index];
  split_t best_split{};

  aabb_t split_bounds{};
  for (auto &refrence :
       std::span(bvh.refrences.begin() + node.as.leaf.first_reference_index,
                 bvh.refrences.begin() + node.as.leaf.first_reference_index +
                     node.refrence_count)) {
    split_bounds.grow(refrence.aabb.center());
  }

  for (uint32_t axis = 0; axis < 3; axis++) {

    if (split_bounds.max[axis] == split_bounds.min[axis])
      continue;

    object_bin_t *p_bins = reinterpret_cast<object_bin_t *>(
        alloca(sizeof(object_bin_t) * o.object_samples));
    std::span bins{p_bins, o.object_samples};

    for (auto &bin : bins)
      bin = object_bin_t{.aabb = {.min = {0, 0, 0}, .max = {0, 0, 0}}};

    float scale = float(o.object_samples) /
                  (split_bounds.max[axis] - split_bounds.min[axis]);
    for (auto &refrence :
         std::span(bvh.refrences.begin() + node.as.leaf.first_reference_index,
                   bvh.refrences.begin() + node.as.leaf.first_reference_index +
                       node.refrence_count)) {
      float center = refrence.aabb.center()[axis];
      horizon_assert(split_bounds.min[axis] <= center,
                     "center is out of bounds! this shouldnt happen.");
      horizon_assert(split_bounds.max[axis] >= center,
                     "center is out of bounds! this shouldnt happen.");
      uint32_t bin_index = std::min(
          o.object_samples - 1,
          static_cast<uint32_t>((center - split_bounds.min[axis]) * scale));

      bins[bin_index].primitive_count++;
      bins[bin_index].aabb.grow(refrence.aabb);
    }

    float *p_left_area = reinterpret_cast<float *>(
        alloca(sizeof(float) * (o.object_samples - 1)));
    float *p_right_area = reinterpret_cast<float *>(
        alloca(sizeof(float) * (o.object_samples - 1)));
    uint32_t *p_left_count = reinterpret_cast<uint32_t *>(
        alloca(sizeof(uint32_t) * (o.object_samples - 1)));
    uint32_t *p_right_count = reinterpret_cast<uint32_t *>(
        alloca(sizeof(uint32_t) * (o.object_samples - 1)));

    std::span left_area{p_left_area, o.object_samples - 1};
    std::span right_area{p_left_area, o.object_samples - 1};
    std::span left_count{p_left_count, o.object_samples - 1};
    std::span right_count{p_left_count, o.object_samples - 1};
    for (uint32_t i = 0; i < o.object_samples - 1; i++) {
      left_area[i] = 0;
      right_area[i] = 0;
      left_count[i] = 0;
      right_count[i] = 0;
    }
    aabb_t left_aabb{}, right_aabb{};
    uint32_t left_sum = 0, right_sum = 0;
    for (uint32_t i = 0; i < o.object_samples - 1; i++) {
      left_sum += bins[i].primitive_count;
      right_sum += bins[o.object_samples - i - 1].primitive_count;

      left_count[i] = left_sum;
      right_count[o.object_samples - i - 2] = right_sum;

      left_aabb.grow(bins[i].aabb);
      right_aabb.grow(bins[o.object_samples - i - 1].aabb);

      left_area[i] = left_aabb.area();
      right_area[o.object_samples - i - 2] = right_aabb.area();
    }
    scale = (split_bounds.max[axis] - split_bounds.min[axis]) /
            static_cast<float>(o.object_samples);
    for (uint32_t i = 0; i < o.object_samples - 1; i++) {
      float cost =
          greedy_cost_of_node(left_count[i], right_count[i], left_area[i],
                              right_area[i], node.aabb.area(), o);
      if (cost < best_split.cost) {
        // TODO: add left and right aabb to split
        best_split.cost = cost;
        best_split.axis = axis;
        best_split.position = split_bounds.min[axis] + scale * (i + 1);
      }
    }
  }

  return best_split;
}

inline float cost_of_node(bvh_t &bvh, uint32_t node_index, const options_t &o) {
  node_t &node = bvh.nodes[node_index];
  if (node.is_leaf)
    return o.primitive_intersection_cost * node.refrence_count;
  float cost = 0;
  uint32_t i = 0;
  for (auto &child :
       std::span{bvh.nodes.begin() + node.as.internal.first_child_index,
                 bvh.nodes.begin() + node.as.internal.first_child_index +
                     node.as.internal.children_count}) {
    cost += child.aabb.area() *
            cost_of_node(bvh, node.as.internal.first_child_index + i++, o);
  }
  return cost;
}

inline uint32_t partition_node_indices(bvh_t &bvh, uint32_t node_index,
                                       const split_t &split) {
  node_t &node = bvh.nodes[node_index];
  horizon_assert(node.is_leaf, "node is not leaf");
  auto middle = std::partition(
      bvh.refrences.begin() + node.as.leaf.first_reference_index,
      bvh.refrences.begin() + node.as.leaf.first_reference_index +
          node.refrence_count,
      [&](const refrence_t &refrence) {
        return refrence.aabb.center()[split.axis] < split.position;
      });
  return std::distance(bvh.refrences.begin(), middle);
}

inline void try_split_node(bvh_t &bvh, uint32_t node_index, const options_t &o);

inline bool object_split_node(bvh_t &bvh, uint32_t node_index,
                              const split_t &split, const options_t &o) {
  if (split.axis == 3) {
    horizon_warn("invalid split axis");
    return false;
  }

  node_t &node = bvh.nodes[node_index];

  uint32_t right_first_primitive_index =
      partition_node_indices(bvh, node_index, split);

  if (node.as.leaf.first_reference_index == right_first_primitive_index |
      node.as.leaf.first_reference_index + node.refrence_count ==
          right_first_primitive_index) {
    horizon_warn("split didnt partition primitives");
    return false;
  }

  node_t left{}, right{};

  left.is_leaf = true;
  left.refrence_count =
      right_first_primitive_index - node.as.leaf.first_reference_index;
  left.as.leaf.first_reference_index = node.as.leaf.first_reference_index;

  right.is_leaf = true;
  right.refrence_count = node.as.leaf.first_reference_index +
                         node.refrence_count - right_first_primitive_index;
  right.as.leaf.first_reference_index = right_first_primitive_index;

  node.is_leaf = false;

  check(number_of_bits_required_for_number(bvh.nodes.size() + 2) <
            FIRST_INDEX_BITS_SIZE,
        "number doesnt fit in {} bits", FIRST_INDEX_BITS_SIZE);
  node.as.internal.first_child_index = bvh.nodes.size();
  node.as.internal.children_count = 2;

  uint32_t left_node_index = bvh.nodes.size();
  bvh.nodes.push_back(left);

  uint32_t right_node_index = bvh.nodes.size();
  bvh.nodes.push_back(right);

  update_node_bounds(bvh, left_node_index);
  update_node_bounds(bvh, right_node_index);

  try_split_node(bvh, left_node_index, o);
  try_split_node(bvh, right_node_index, o);

  return true;
}

inline bool spatial_split_node(bvh_t &bvh, uint32_t node_index,
                               const split_t &split, const options_t &o) {
  if (split.axis == 3) {
    horizon_warn("invalid split axis");
    return false;
  }

  return true;
}

inline void try_split_node(bvh_t &bvh, uint32_t node_index,
                           const options_t &o) {
  node_t &node = bvh.nodes[node_index];

  const split_t split = find_best_object_split(bvh, node_index, o);

  float no_split_cost = cost_of_node(bvh, node_index, o);
  if (split.cost < no_split_cost) {
    bool did_split = object_split_node(bvh, node_index, split, o);
  }
}

// TODO: remove inline
inline bvh_t build_bvh2(aabb_t *aabbs, uint32_t primitive_count,
                        const options_t &o) {
  bvh_t bvh{};

  bvh.refrences.reserve(primitive_count);
  for (uint32_t i = 0; i < primitive_count; i++)
    bvh.refrences.push_back(refrence_t{.aabb = aabbs[i], .primitive_index = i});

  node_t root = node_t{};
  root.is_leaf = true;
  root.refrence_count = primitive_count;
  root.as.leaf.first_reference_index = 0;
  bvh.nodes.push_back(root);
  bvh.parents.push_back(invalid_index);

  update_node_bounds(bvh, 0);
  try_split_node(bvh, 0, o);

  // TODO: fix all non leaf refrence counts

  return bvh;
}

} // namespace bvh

#endif

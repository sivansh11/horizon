#include "../raytracing/raytrace_core.slang"

#define MAX_ENTRIES_PER_NODE 1024

struct DispatchIndirectCmd {
  uint32_t x, y, z;
}

struct oct_tree_node_entry_t {
  float3 color;
};

struct oct_tree_node_t {
  aabb_t aabb;
  uint32_t child_indices[8];
  uint32_t spin_lock[8];
  uint32_t did_entry;
  oct_tree_node_entry_t entry;
  // oct_tree_node_entry_t entries[MAX_ENTRIES_PER_NODE];
  // uint32_t num_entries;
};

struct push_constant_t {
  float3 *throughput;
  uint32_t *num_rays;
  uint32_t *new_num_rays;
  DispatchIndirectCmd *dispatch_indirect_cmd;
  ray_data_t *ray_datas;
  ray_data_t *new_ray_datas;
  bvh_t *bvh;
  triangle_t *triangles;
  camera_t *camera;
  hit_t *hits;
  uint32_t width;
  uint32_t height;
  uint32_t bounce_id;
};


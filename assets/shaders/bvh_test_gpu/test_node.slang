#include "../raytracing/raytrace_core.slang"

struct push_constant_t {
    node_t *node;
}

// public struct node_t {
//     aabb_t aabb;
//     uint32_t is_leaf : 1;
//     uint32_t primitive_count : 31;
//     uint32_t first_primitive_index_or_first_child_index : FIRST_INDEX_BITS_SIZE;
//     uint32_t children_count: 4;    // do not use
// };

[vk::push_constant] push_constant_t pc;

[shader("compute")]
[numthreads(1, 1, 1)]
void compute_main(uint3 dispatch_thread_id: SV_DispatchThreadID) {
    printf("node:\n");
    printf("\taabb:\n\t\t%v3f\n\t\t%v3f\n", pc.node.aabb.min, pc.node.aabb.max);
    if (bool(pc.node.is_leaf)) {
        printf("\tnode is leaf\n");
    } else {
        printf("\tnode is not leaf\n");
    }
    uint32_t primitive_count = pc.node.primitive_count;
    uint32_t first_primitive_index_or_first_child_index = pc.node.first_primitive_index_or_first_child_index;
    uint32_t children_count = pc.node.children_count;
    printf("\t%u\n", primitive_count);
    printf("\t%u\n", first_primitive_index_or_first_child_index);
    printf("\t%u\n", children_count);

}


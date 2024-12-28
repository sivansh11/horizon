#include "common.slang"

[vk::push_constant] push_constant_t pc;

[shader("compute")]
[numthreads(1, 1, 1)]
void compute_main() {
    // printf("%u rays ", *pc.new_num_rays);
    pc.dispatch_indirect_cmd.x = ((*pc.new_num_rays) + 64 - 1) / 64;
    pc.dispatch_indirect_cmd.y = 1;
    pc.dispatch_indirect_cmd.z = 1;
    // printf(": %u dispatch\n", pc.dispatch_indirect_cmd.x);
}
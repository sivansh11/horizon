#include "../raytracing/raytracing.slang"

static const uint32_t work_group_width = 8;
static const uint32_t work_group_height = 8;

[shader('compute')]
[numthreads(work_group_width, work_group_height, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchTreadID, uint group_index : SV_GroupIndex) {

}

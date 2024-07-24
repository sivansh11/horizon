struct input_t {
    uint32_t i : 8;
    uint32_t j : 24;
};

struct output_t {
    uint32_t i;
    uint32_t j;
};

#define MAX_SLOTS 1000

struct param_t {
    void *data;
};

uniform Texture2D sampled_images[MAX_SLOTS];
uniform SamplerState sampler[MAX_SLOTS];
uniform RWStructuredBuffer<param_t> buffers;

[shader("compute")]
[numthreads(1, 1, 1)]
void main(uint3 dispatchThreadID: SV_DispatchThreadID, uint groupIndex: SV_GroupIndex) {
    // buffer 0 is input
    // buffer 1 is output
    input_t *input = (input_t *)(buffers[0].data);
    output_t *output = (output_t *)(buffers[1].data);


    output->i = input->i;
    output->j = input->j;
}
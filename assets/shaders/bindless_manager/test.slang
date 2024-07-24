#define define_alias(name) \
    struct param_##name {  \
        name *data[1000];  \
    };                     \
    [[vk::binding(0, 0)]] uniform ConstantBuffer<param_##name> buffer_##name

#define get(name, index) \
    buffer_##name.data[index];

[[vk::binding(1, 0)]] uniform SamplerState samplers[1000];
[[vk::binding(2, 0)]] uniform Texture2D images[1000];

struct dummy_t {
    int i;
};

define_alias(dummy_t);



struct input_t {
    uint32_t i;
    uint32_t j;
};

struct output_t {
    uint32_t i;
    uint32_t j;
};

define_alias(input_t);
define_alias(output_t);


[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain(uint3 dispatchThreadID: SV_DispatchThreadID, uint groupIndex: SV_GroupIndex) {
    input_t *input = get(input_t, 0);
    output_t *output = get(output_t, 1);

    output->i = input->i;
    output->j = input->j;
}

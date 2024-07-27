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

struct per_fragment_t {
    float2 uv;
    uint tex_id;
};

struct vertex_t {
    float3 position;
    float3 normal;
    float2 uv;
    float3 tangent;
    float3 bi_tangent;
};

struct fragment_t {
    float4 o_color : COLOR0;
};

[shader("fragment")]
fragment_t fragmentMain(per_fragment_t per_fragment) : SV_Target {
    fragment_t fragment;

    fragment.o_color = images[0].Sample(samplers[0], per_fragment.uv);

    return fragment;
}
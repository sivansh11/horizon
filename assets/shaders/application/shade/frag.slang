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

struct material_t {
    uint32_t albedo_id;
};
define_alias(material_t);
#define MATERIAL_SLOT_ID 998

struct per_fragment_t {
    float2 uv;
};

struct fragment_t {
    float3 o_color : COLOR0;
};

[shader("fragment")]
fragment_t fragmentMain(per_fragment_t per_fragment) : SV_Target {
    fragment_t fragment;

    float2 uv = float2(per_fragment.uv.x, per_fragment.uv.y);

    fragment.o_color = images[1].Sample(samplers[0], uv).xyz;

        printf("%u", uint32_t(images[4].Sample(samplers[1], uv).x));

    // fragment.o_color.x = images[4].Sample(samplers[0], uv).x;
    // if (per_fragment.uv.x == 0) {
    //     printf("%u", uint32_t(images[4].Sample(samplers[1], uv).x));
    // }

    // material_t *material_buffer = get(material_t, MATERIAL_SLOT_ID);

    // if (material_id > 0) {
    //     material_t material = material_buffer[material_id];
    //     float2 fragment_uv = images[3].Sample(samplers[0], uv).xy;
    //     fragment.o_color = images[material.albedo_id].Sample(samplers[0], fragment_uv).xyz;
    // } else {
    //     fragment.o_color = float3(0, 0, 0);
    // }


    return fragment;
}
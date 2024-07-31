[[vk::binding(0, 0)]] uniform SamplerState samplers[1000];
[[vk::binding(1, 0)]] uniform Texture2D images[1000];
[[vk::binding(1, 0)]] uniform Texture2D<uint4> images_uint4[1000];

struct vertex_t {
    float3 position;
    float3 normal;
    float2 uv;
    float3 tangent;
    float3 bi_tangent;
};

struct transform_t {
    float4x4 model;
    float4x4 inv_model;
};

struct material_t {
    uint32_t albedo;
};

struct camera_t {
    float4x4 view;
    float4x4 inv_view;
    float4x4 projection;
    float4x4 inv_projection;
};

struct push_constant_t {
    vertex_t *vertices;
    uint32_t *indices;
    uint32_t transform_index;
    uint32_t material_index;
    transform_t *transforms;
    material_t *materials;
    camera_t *camera;
};

[[vk::push_constant]] push_constant_t push_constant;

struct per_fragment_t {
    float2 uv;
};

struct fragment_t {
    float4 o_color : COLOR0;
};

[shader("fragment")]
fragment_t fragmentMain(per_fragment_t per_fragment) : SV_Target {
    fragment_t fragment;

    float2 uv = float2(per_fragment.uv.x, per_fragment.uv.y);

    uint material_index = images_uint4[4].Sample(samplers[1], uv).x;

    if (material_index != 0) {
        fragment.o_color.xyz = images[NonUniformResourceIndex(push_constant.materials[material_index].albedo)].Sample(samplers[0], images[3].Sample(samplers[0], uv).xy).xyz;
    } else {
        fragment.o_color.xyz = { 0, 0, 0 };
    }

    fragment.o_color.w = 1;
    
    return fragment;
}   
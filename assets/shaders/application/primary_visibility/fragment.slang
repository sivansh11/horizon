[[vk::binding(0, 0)]] uniform SamplerState samplers[1000];
[[vk::binding(1, 0)]] uniform Texture2D images[1000];

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
    float3 position;
    float3 normal;
    float2 uv;
};

struct fragment_t {
    float3 o_position : COLOR0;
    float3 o_normal : COLOR1;
    float2 o_uv : COLOR2;
    uint32_t o_material_id : COLOR3;
};

[shader("fragment")]
fragment_t fragmentMain(per_fragment_t per_fragment) : SV_Target {
    fragment_t fragment;

    fragment.o_position = per_fragment.position;
    fragment.o_normal = per_fragment.normal;
    fragment.o_uv = per_fragment.uv;
    fragment.o_material_id = push_constant.material_index;

    return fragment;
}
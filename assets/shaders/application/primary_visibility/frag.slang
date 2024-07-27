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

struct vertex_t {
    float3 position;
    float3 normal;
    float2 uv;
    float3 tangent;
    float3 bi_tangent;
};

struct mesh_t {
    uint32_t material_id;
    uint32_t model_transform_id;
    vertex_t *vertices;
    uint32_t *indicies;
};

[[vk::push_constant]] mesh_t mesh;

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
    fragment.o_material_id = mesh.material_id;

    return fragment;
}
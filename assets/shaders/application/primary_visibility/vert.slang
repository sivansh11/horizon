#define define_alias(name) \
    struct param_##name {                      \
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

struct model_transform_t {
    float4x4 model;
    float4x4 inv_model;
};
define_alias(model_transform_t);
#define MODEL_TRANSFORM_SLOT_ID 999

struct material_t {
    uint32_t albedo_id;
};
define_alias(material_t);
#define MATERIAL_SLOT_ID 998

struct camera_t {
    float4x4 view;
    float4x4 inv_view;
    float4x4 projection;
    float4x4 inv_projection;
};
define_alias(camera_t);
#define CAMERA_SLOT_ID 997

struct per_fragment_t {
    float3 position;
    float3 normal;
    float2 uv;
};

struct vertex_shader_output_t {
    per_fragment_t per_fragment;
    float4 sv_position : SV_Position;
};

[shader("vertex")]
vertex_shader_output_t vertexMain(uint32_t index: SV_VertexID) {
    vertex_shader_output_t output;

    vertex_t *vertices = mesh.vertices;
    uint32_t *indices = mesh.indicies;
    vertex_t vertex = vertices[indices[index]];

    camera_t *camera = get(camera_t, CAMERA_SLOT_ID);

    model_transform_t *model_transform_buffer = get(model_transform_t, MODEL_TRANSFORM_SLOT_ID);
    model_transform_t model_transform = model_transform_buffer[mesh.model_transform_id];

    float4 world_position = mul(float4(vertex.position, 1), model_transform.model);

    output.sv_position = mul(mul(world_position, camera->view), camera->projection);
    output.per_fragment.position = world_position.xyz;
    output.per_fragment.normal = mul(float4(vertex.normal, 0), model_transform.model).xyz;
    output.per_fragment.uv = vertex.uv;


    return output;
}
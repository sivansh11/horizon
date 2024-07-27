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

struct per_fragment_t {
    float2 uv;
    uint tex_id;
};

struct vertex_shader_output_t {
    per_fragment_t per_fragment;
    float4 sv_position : SV_Position;
};

struct vertex_t {
    float3 position;
    float3 normal;
    float2 uv;
    float3 tangent;
    float3 bi_tangent;
};

define_alias(vertex_t);
define_alias(uint32_t);

[shader("vertex")]
vertex_shader_output_t vertexMain(uint32_t index : SV_VertexID) {
    vertex_shader_output_t output;

    vertex_t *vertices = get(vertex_t, 0);
    uint32_t *indices = get(uint32_t, 1);
    vertex_t vertex = vertices[indices[index]];

    output.sv_position = float4(vertex.position, 1);
    output.per_fragment.uv = vertex.uv;
    output.per_fragment.tex_id = uint(vertex.normal.x);

    return output;
}
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

struct per_model_t {
    StructuredBuffer<uint32_t> indices;
    StructuredBuffer<vertex_t> vertices;
};
uniform ParameterBlock<per_model_t> per_model;
struct common_t {
    SamplerState sampler_state;
    Texture2D tex[2];
};
uniform ParameterBlock<common_t> common;

[shader("vertex")]
vertex_shader_output_t vertexMain(uint32_t index : SV_VertexID) {
    vertex_shader_output_t output;

    vertex_t vertex = per_model.vertices[per_model.indices[index]];

    output.sv_position = float4(vertex.position, 1);
    output.per_fragment.uv = vertex.uv;
    output.per_fragment.tex_id = uint(vertex.normal.x);

    return output;
}
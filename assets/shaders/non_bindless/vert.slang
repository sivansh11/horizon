struct per_fragment_t {
    float2 uv;
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

uniform StructuredBuffer<uint32_t> indices;
uniform StructuredBuffer<vertex_t> vertices;
uniform SamplerState sampler_state;
uniform Texture2D tex;

[shader("vertex")]
vertex_shader_output_t vertexMain(uint32_t index : SV_VertexID) {
    vertex_shader_output_t output;

    vertex_t vertex = vertices[indices[index]];

    output.sv_position = float4(vertex.position, 1);
    output.per_fragment.uv = vertex.uv;

    return output;
}
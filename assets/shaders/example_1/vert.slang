struct per_fragment_t {
    float2 uv;
};

struct vertex_shader_output_t {
    per_fragment_t per_fragment;
    float4 sv_position : SV_POSITION;
};

struct vertex_t {
    float3 position;
    float3 normal;
    float2 uv;
    float3 tangent;
    float3 bi_tangent;
};

[[vk::binding(0, 0)]] uniform StructuredBuffer<uint32_t> indices;
[[vk::binding(1, 0)]] uniform StructuredBuffer<vertex_t> vertices;
[[vk::binding(2, 0)]] uniform SamplerState sampler;
[[vk::binding(3, 0)]] uniform Texture2D texture;

[shader("vertex")]
vertex_shader_output_t vertex_main(uint32_t index: SV_VertexID) {
    vertex_shader_output_t output;

    vertex_t vertex = vertices[indices[index]];

    output.sv_position = float4(vertex.position, 1);
    output.per_fragment.uv = vertex.uv;

    return output;
}
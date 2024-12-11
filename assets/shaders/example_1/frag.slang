struct per_fragment_t {
    float2 uv;
};

struct vertex_t {
    float3 position;
    float3 normal;
    float2 uv;
    float3 tanget;
    float3 bi_tangent;
};

struct fragment_t {
    float4 o_color : COLOR0;
};

[[vk::binding(0, 0)]] uniform StructuredBuffer<uint32_t> indices;
[[vk::binding(1, 0)]] uniform StructuredBuffer<vertex_t> vertices;
[[vk::binding(2, 0)]] uniform SamplerState sampler;
[[vk::binding(3, 0)]] uniform Texture2D texture;

[shader("fragment")]
fragment_t fragment_main(per_fragment_t per_fragment) : SV_Target {
    fragment_t fragment;
    fragment.o_color = texture.Sample(sampler, per_fragment.uv);
    return fragment;
}

struct per_fragment_t {
    float2 uv;
};

struct vertex_t {
    float3 position;
    float3 normal;
    float2 uv;
    float3 tangent;
    float3 bi_tangent;
};

struct fragment_t {
    float4 o_color : COLOR0;
};

uniform StructuredBuffer<uint32_t> indices;
uniform StructuredBuffer<vertex_t> vertices;
uniform SamplerState sampler_state;
uniform Texture2D tex;

// TODO: sample a image

[shader("fragment")]
fragment_t fragmentMain(per_fragment_t per_fragment) : SV_Target {
    fragment_t fragment;

    fragment.o_color = tex.Sample(sampler_state, per_fragment.uv);

    return fragment;
}
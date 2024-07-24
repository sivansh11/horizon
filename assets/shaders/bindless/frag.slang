struct per_fragment_t {
    float2 uv;
    uint tex_id;
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

[shader("fragment")]
fragment_t fragmentMain(per_fragment_t per_fragment) : SV_Target {
    fragment_t fragment;

    fragment.o_color = common.tex[per_fragment.tex_id].Sample(common.sampler_state, per_fragment.uv);

    return fragment;
}
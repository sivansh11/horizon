import vert;

struct pixel_t {
    float4 color : COLOR0;
};

uniform Texture2D<float4> tex;
uniform SamplerState sampler_state;

[shader("fragment")]
pixel_t fragmentMain(per_fragment_t per_fragment) : SV_Target {
    pixel_t p;

    // p.color = { 1, 0, 1, 1 };
    p.color =  tex.Sample(sampler_state, per_fragment.uv);

    return p;
}
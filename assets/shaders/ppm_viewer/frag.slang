import vert;

struct pixel_t {
    float4 color : COLOR0;
};
uniform Sampler2D screen;

[shader("fragment")]
pixel_t fragmentMain(per_fragment_t per_fragment) : SV_Target {
    pixel_t p;
    p.color = screen.Sample(per_fragment.uv);
    return p;
}
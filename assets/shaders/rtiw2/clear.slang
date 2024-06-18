struct uniform_t {
    uint32_t width, height;
    float4 clear_color;
};
uniform uniform_t uni;
uniform RWTexture2D<float4> o_color;

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 dispatchThreadID: SV_DispatchThreadID) {

    if (dispatchThreadID.x >= uni.width) return;
    if (dispatchThreadID.y >= uni.height) return;

    float4 color = { 0, 0, 0, 0 };

    o_color[uint2(dispatchThreadID.x, dispatchThreadID.y)] = uni.clear_color;
}
[[vk::binding(0, 0)]]
uniform Texture2D textures[1000];
[[vk::binding(1, 0)]]
uniform SamplerState samplers[1000];

struct push_constant_t {
  float4 *result;
};

[[vk::push_constant]]
push_constant_t pc;

[shader("compute")]
[numthreads(1, 1, 1)]
void compute_main() {
  *pc.result = textures[0].SampleLevel(samplers[0], float2(0.5, 0.5), 0);
}

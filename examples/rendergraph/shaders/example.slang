struct vertex_stage_output_t {
  float4 sv_position : SV_Position;
  float2 uv;
};

static const float2 positions[6] = {
  float2(-1, -1),
  float2(-1,  1),
  float2( 1,  1),
  float2(-1, -1),
  float2( 1,  1),
  float2( 1, -1),
};

static const float2 uv[6] = {
  float2(0, 1),
  float2(0, 0),
  float2(1, 0),
  float2(0, 1),
  float2(1, 0),
  float2(1, 1),

  // float2(0, 0),
  // float2(0, 1),
  // float2(1, 1),
  // float2(0, 0),
  // float2(1, 1),
  // float2(1, 0),
};

[shader("vertex")]
vertex_stage_output_t vertex_main(uint32_t id: SV_VertexID) {
  vertex_stage_output_t o;
  o.uv = uv[id];
  o.sv_position = float4(positions[id], 0, 1);
  return o;
}

struct fragment_t {
  float4 color : COLOR0;
};

// binding set
[vk::binding(0, 0)]
uniform Texture2D image;
[vk::binding(1, 0)]
uniform SamplerState sampler;

[shader("fragment")]
fragment_t fragment_main(float2 uv) {
  fragment_t fragment;
  fragment.color = image.Sample(sampler, uv);
  return fragment;
}

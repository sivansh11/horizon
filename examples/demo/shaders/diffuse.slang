struct camera_t {
  float4x4 view;  
  float4x4 inv_view;  
  float4x4 proj;  
  float4x4 inv_proj;  
};

struct vertex_t {
  float3 position;
  float3 normal;
  float2 uv;
  float3 tangent;
  float3 bi_tangent;
};

struct push_constant_t {
  camera_t *camera;
  vertex_t *vertices;
  uint32_t *indices;
  uint32_t b_diffuse;
  uint32_t b_sampler;
};

[vk::push_constant]
push_constant_t pc;

[vk::binding(0, 0)]
uniform Texture2D textures[1000];
[vk::binding(1, 0)]
uniform SamplerState samplers[1000];

struct vertex_stage_output_t {
  float4 sv_position : SV_Position;
  float2 uv;
};

[shader("vertex")]
vertex_stage_output_t vertex_main(uint32_t id : SV_VertexID) {
  vertex_stage_output_t o;
  vertex_t vertex = pc.vertices[pc.indices[id]];
  o.uv = vertex.uv;
  o.sv_position = mul(mul(float4(vertex.position, 1), pc.camera->view), pc.camera->proj);
  return o;
}

struct fragment_t {
  float4 color : COLOR0;
};

[shader("fragment")]
fragment_t fragment_main(float2 uv) {
  fragment_t fragment;
  fragment.color = textures[pc.b_diffuse].Sample(samplers[pc.b_sampler], uv);
  return fragment;
}

[[vk::binding(0, 0)]] uniform SamplerState samplers[1000];
[[vk::binding(1, 0)]] uniform Texture2D images[1000];

struct vertex_t {
    float3 position;
    float3 normal;
    float2 uv;
    float3 tangent;
    float3 bi_tangent;
};

struct transform_t {
    float4x4 model;
    float4x4 inv_model;
};

struct material_t {
    uint32_t albedo;
};

struct camera_t {
    float4x4 view;
    float4x4 inv_view;
    float4x4 projection;
    float4x4 inv_projection;
};

struct push_constant_t {
    vertex_t *vertices;
    uint32_t *indices;
    uint32_t transform_index;
    uint32_t material_index;
    transform_t *transforms;
    material_t *materials;
    camera_t *camera;
};

[[vk::push_constant]] push_constant_t push_constant;

struct per_fragment_t {
    float3 position;
    float3 normal;
    float2 uv;
};

struct vertex_shader_output_t {
    per_fragment_t per_fragment;
    float4 sv_position : SV_Position;
};

[shader("vertex")]
vertex_shader_output_t vertexMain(uint32_t index: SV_VertexID) {
    vertex_shader_output_t output;

    vertex_t vertex = push_constant.vertices[push_constant.indices[index]];

    transform_t transform = push_constant.transforms[push_constant.transform_index];

    camera_t camera = *push_constant.camera;

    float4 world_position = mul(float4(vertex.position, 1), transform.model);

    output.sv_position = mul(mul(world_position, camera.view), camera.projection);
    output.per_fragment.position = world_position.xyz;
    output.per_fragment.normal = mul(float4(vertex.normal, 0), transform.model).xyz;
    output.per_fragment.uv = vertex.uv;

    printf("%f", transform.model._11);

    return output;
}
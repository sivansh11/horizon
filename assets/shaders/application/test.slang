struct vertex_t {
    float3 position;
    float3 normal;
    float2 uv;
    float3 tangent;
    float3 bi_tangent;
};

struct mesh_t {
    uint32_t material_id;
    uint32_t model_transform_id;
    vertex_t *vertices;
    uint32_t *indicies;
};

[[vk::push_constant]] mesh_t mesh;

[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain(uint3 dispatchThreadID: SV_DispatchThreadID) {
    // printf("%u %u.", mesh.material_id, mesh.model_transform_id);
}
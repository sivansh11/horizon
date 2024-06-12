struct per_fragment_t {
    float2 uv;
};

struct vertex_shader_output_t {
    per_fragment_t per_fragment : per_fragment_t;
    float4 sv_position : SV_Position;
};

static float2 positions[6] = float2[](
    float2(-1, -1),
    float2(-1, 1),
    float2(1, 1),
    float2(-1, -1),
    float2(1, 1),
    float2(1, -1)
);

static float2 uv[6] = float2[](
    float2(0, 0),
    float2(0, 1),
    float2(1, 1),
    float2(0, 0),
    float2(1, 1),
    float2(1, 0)
);

[shader("vertex")]
vertex_shader_output_t vertexMain(uint id: SV_VertexID) {
    vertex_shader_output_t o;
    o.sv_position = float4(positions[id], 0, 1);
    o.per_fragment.uv = uv[id];
    return o;
}

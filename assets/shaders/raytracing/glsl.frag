#version 450

#extension GL_EXT_scalar_block_layout : enable

#include "includes/raytracing_structs.glsl"

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 out_color;

layout (set = 0, binding = 0, scalar) readonly buffer triangle_buffer_t {
    triangle_t triangles[];
};

layout (set = 0, binding = 1, scalar) readonly buffer node_buffer_t {
    node_t nodes[];
};

layout (set = 0, binding = 2, scalar) readonly buffer primitive_index_buffer_t {
    uint primitive_indices[];
};

layout (set = 0, binding = 3, scalar) readonly buffer parent_id_buffer_t {
    uint parent_ids[];
};

layout (set = 1, binding = 0, scalar) uniform camera_buffer_t {
    mat4 projection;
    mat4 inv_projection;
    mat4 view;
    mat4 inv_view;
};

#include "includes/raytracing_functions.glsl"

float col(uint c) {
    c = c % 255;
    return float(c) / 255.f;
}

void main() {
    ray_t ray = create_ray(uv, inv_view);

    hit_t hit = traverse(ray);
    if (hit.primitive_index != uint(-1)) 
        out_color = vec4(col((hit.primitive_index + 1) * 37), col((hit.primitive_index + 1) * 91), col((hit.primitive_index + 1) * 51), 1);
    else
        out_color = vec4(0, 0, 0, 1);
}
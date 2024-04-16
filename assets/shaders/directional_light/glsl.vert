#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 bi_tangent;

layout (location = 0) out vec3 frag_position;
layout (location = 1) out vec3 frag_normal;

layout (set = 0, binding = 0) uniform camera_uniform_t {
    mat4 view;
    mat4 inv_view;
    mat4 projection;
    mat4 inv_projection;
};

// layout (set = 1, binding = 0) uniform set_1_uniform_buffer_t {
//     mat4 model;
//     mat4 inv_model;
// };

void main() {
    frag_position = vec3(vec4(position, 1));
    frag_normal = normal;

    gl_Position = projection * view * vec4(frag_position, 1);
}
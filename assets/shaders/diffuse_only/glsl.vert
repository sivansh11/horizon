#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 bi_tangent;

layout (location = 0) out vec2 out_uv;

layout (set = 0, binding = 0) uniform camera_t {
    mat4 projection;
    mat4 inv_projection;
    mat4 view;
    mat4 inv_view;
} camera;

layout (set = 1, binding = 0) uniform object_t {
    mat4 model;
    mat4 inv_model;
} object;

void main() {
    vec4 world_position = object.model * vec4(position, 1);
    gl_Position = camera.projection * camera.view * world_position;
    out_uv = uv;
}
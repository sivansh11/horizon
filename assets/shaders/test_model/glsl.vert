#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 bi_tangent;

void main() {
    vec4 world_position = vec4(position, 1);
    gl_Position = world_position;
}

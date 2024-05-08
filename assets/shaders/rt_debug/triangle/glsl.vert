#version 450

layout (location = 0) in vec3 position;

layout (set = 0, binding = 0) uniform camera_t {
    mat4 projection;
    mat4 inv_projection;
    mat4 view;
    mat4 inv_view;
};

void main() {
    vec4 world_position = vec4(position, 1);
    gl_Position = projection * view * world_position;
}

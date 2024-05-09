#version 450

layout (location = 0) out vec4 out_color;

layout (set = 1, binding = 1) uniform color_t {
    vec3 color;
};

void main() {
    // out_color = vec4(color, 0.005);
    out_color = vec4(0.0005);
}
#version 450

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 out_color;

layout (set = 2, binding = 0) uniform sampler2D albedo;

void main() {
    out_color = texture(albedo, uv);
}
#version 450

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 out_color;

layout (set = 2, binding = 0) uniform sampler2D albedo;

void main() {
    vec4 color = texture(albedo, uv);
    if (color.a < 0.05) discard;
    out_color = color;
}
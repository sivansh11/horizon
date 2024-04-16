#version 450 

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;

layout (location = 0) out vec4 out_color;

layout (set = 1, binding = 0) uniform additional_uniform_t {
    vec3 view_position;
    vec3 direction;
};

void main() {
    vec3 ambient = vec3(0.1);
    vec3 norm = normalize(normal);
    vec3 light_direction = normalize(-direction);
    float diff = max(dot(norm, light_direction), 0);
    vec3 diffuse = vec3(diff);
    out_color = vec4(ambient + diffuse, 1);
}
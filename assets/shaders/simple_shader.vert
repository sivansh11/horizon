#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;

layout (location = 0) out vec3 fragColor;

layout (push_constant) uniform Push
{
    mat4 transform;
    mat4 normalMatrix;
} push;

void main()
{
    // gl_Position = vec4(push.transform * position + push.offset, 0.0, 1.0);
    gl_Position = push.transform * vec4(position, 1.0);

    // mat3 normalMatrix = transpose(inverse(mat3(push.modelMatrix)));
    // vec3 normalWorldSpace = normalize(normalMatrix * normal);

    vec3 normalWorldSpace = normalize(mat3(push.normalMatrix) * normal);

    float lightIntensity = AMBIENT + max(dot(normalWorldSpace, DIRECTION_TO_LIGHT), 0);

    fragColor = color * lightIntensity;
}
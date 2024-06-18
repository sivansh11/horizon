#include "my_types.slang"

float length2(vec2 v) {
    return v.x * v.x + v.y * v.y;
}

float length2(vec3 v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

float length2(vec4 v) {
    return v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w;
}
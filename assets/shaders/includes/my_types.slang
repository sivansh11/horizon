typedef float2 vec2;
typedef float3 vec3;
typedef float4 vec4;

typedef float2x2 mat2;
typedef float3x3 mat3;
typedef float4x4 mat4;

extension float3 {
    __init(float4 v) {
        this.x = v.x;
        this.y = v.y;
        this.z = v.z;
    }
};

vec4 operator *(mat4 m, vec4 v) {
    return mul(m, v);
}

vec4 operator *(vec4 v, mat4 m) {
    return mul(v, m);
}
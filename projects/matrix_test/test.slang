
uniform struct uniform_t {
    float4x4 *p_result;
} uniform;

float4 operator *(float4x4 mat, float4 vec) {
    return mul(mat, vec);
}

#define IDENTITY_MATRIX float4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)

// code taken from glm
float4x4 translate(float4x4 m, float3 v) {
    float4x4 result = m;
    result[3] = m[0] * v[0] + m[1] * v[1] + m[2] * v[2] + m[3]; 
    return result;
}

[shader("compute")]
[numthreads(1,1,1)]
void computeMain(uint3 threadId : SV_DispatchThreadID)
{
    uniform.p_result[0] = translate( IDENTITY_MATRIX, { 1, 2, 3 });
}
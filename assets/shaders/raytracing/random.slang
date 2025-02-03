// uint pcg_hash(uint seed) {
//     uint state = seed * 747796405u + 2891336453u;
//     uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
//     return (word >> 22u) ^ word;
// }

// Used to advance the PCG state.
uint rand_pcg(inout uint rng_state) {
    uint state = rng_state;
    rng_state = rng_state * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// uniform distribution
float rand(inout uint state) {
    uint x = rand_pcg(state);
    state = x;
    return float(x) * reinterpret<float>(0x2f800004u);
}

float rand_normal(inout uint seed) {
    float theta = 2 * 3.1415926 * rand(seed);
    float rho = sqrt(-2 * log(rand(seed)));
    return rho * cos(theta);
}

float3 rand_float3(inout uint seed) {
    return { rand(seed), rand(seed), rand(seed) };
}

float2 rand_float2(inout uint seed) {
    return { rand(seed), rand(seed) };
}

// probably should be hidden
float3 rand_float3_in_unit_sphere(inout uint seed) {
    uint32_t count = 0;
    while (true) {
        count++;
        if (count > 100) return { 0, 0, 1 };
        float3 r = rand_float3(seed) * 2.f - 1.f;
        if (length(r) < 1) return r;
    }
    // float theta = 2 * 3.1415926 * rand(seed);
    // float rho = sqrt(-2 * log(rand(seed)));
    // return rho * cos(theta);
}

float3 rand_float3_unit_sphere(inout uint seed) {
    return normalize(rand_float3_in_unit_sphere(seed));
    // return normalize(float3(rand_normal_distribution(seed), rand_normal_distribution(seed), rand_normal_distribution(seed)));
}

float3 rand_float3_hemisphere(inout uint seed, float3 normal) {
    float3 unit_sphere = rand_float3_unit_sphere(seed);
    if (dot(unit_sphere, normal) > 0.0) return unit_sphere;
    return -unit_sphere;
}
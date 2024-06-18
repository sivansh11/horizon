#include "my_types.slang"

uint pcg_hash(uint seed) {
    uint state = seed * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// Used to advance the PCG state.
uint rand_pcg(inout uint rng_state) {
    uint state = rng_state;
    rng_state = rng_state * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// Advances the prng state and returns the corresponding random float.
float rand(inout uint state) {
    uint x = rand_pcg(state);
    state = x;
    return float(x) * reinterpret<float>(0x2f800004u);
}

vec3 rand_vec3(inout uint seed) {
    return { rand(seed), rand(seed), rand(seed) };
}

float2 rand_float2(inout uint seed) {
    return { rand(seed), rand(seed) };
}

// probably should be hidden
vec3 rand_vec3_in_unit_sphere(inout uint seed) {
    while (true) {
        vec3 r = rand_vec3(seed) * 2.f - 1.f;
        if (length(r) < 1) return r;
    }
}

vec3 rand_vec3_unit_sphere(inout uint seed) {
    return normalize(rand_vec3_in_unit_sphere(seed));
}

vec3 rand_vec3_hemisphere(inout uint seed, vec3 normal) {
    vec3 unit_sphere = rand_vec3_unit_sphere(seed);
    if (dot(unit_sphere, normal) > 0.0) return unit_sphere;
    return -unit_sphere;
}
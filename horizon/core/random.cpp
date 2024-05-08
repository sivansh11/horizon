#include "random.hpp"

namespace core {

rng_t::rng_t(uint32_t seed) : _engine(seed), _distribution_0_to_1(0.f, 1.f) {}

float rng_t::randomf() {
    return _distribution_0_to_1(_engine);
}

glm::vec2 rng_t::random2f() {
    return { randomf(), randomf() };
}

glm::vec3 rng_t::random3f() {
    return { randomf(), randomf(), randomf() };
}

glm::vec4 rng_t::random4f() {
    return { randomf(), randomf(), randomf(), randomf() };
}

} // namespace core

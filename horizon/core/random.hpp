#ifndef CORE_RANDOM_HPP
#define CORE_RANDOM_HPP

#include <glm/glm.hpp>

#include <random>

namespace core {

class rng_t {
public:
    rng_t(uint32_t seed = 0);

    float randomf();
    glm::vec2 random2f();
    glm::vec3 random3f();
    glm::vec4 random4f();

private:
    std::mt19937 _engine;
    std::uniform_real_distribution<float> _distribution_0_to_1;
};

} // namespace core

#endif
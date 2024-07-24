#ifndef CORE_MATH_HPP
#define CORE_MATH_HPP

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

namespace core {

using namespace glm;

static const float infinity = std::numeric_limits<float>::max();

} // namespace core

#endif
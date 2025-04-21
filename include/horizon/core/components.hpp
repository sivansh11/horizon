#ifndef CORE_COMPONENTS_HPP
#define CORE_COMPONENTS_HPP

#include "math.hpp"

namespace core {

struct camera_t {
  mat4 view;
  mat4 projection;
};

struct transform_t {
  vec3 translation{0.f, 0.f, 0.f};
  vec3 rotation{0.f, 0.f, 0.f};
  vec3 scale{1.f, 1.f, 1.f};

  core::mat4 mat4() const;
};

} // namespace core

#endif // !CORE_COMPONENTS_HPP

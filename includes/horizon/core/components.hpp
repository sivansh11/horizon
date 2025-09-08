#ifndef CORE_COMPONENTS_HPP
#define CORE_COMPONENTS_HPP

#include "math/math.hpp"

namespace core {

struct camera_t {
  math::mat4 view;
  math::mat4 inv_view;
  math::mat4 projection;
  math::mat4 inv_projection;
  void       update();
  math::vec3 position() const;
};

struct transform_t {
  math::vec3 translation{0.f, 0.f, 0.f};
  math::vec3 rotation{0.f, 0.f, 0.f};
  math::vec3 scale{1.f, 1.f, 1.f};

  math::mat4 mat4() const;
};

}  // namespace core

#endif  // !CORE_COMPONENTS_HPP

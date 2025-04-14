#ifndef CORE_COMPONENTS_HPP
#define CORE_COMPONENTS_HPP

#include "math.hpp"

namespace core {

struct camera_t {
  mat4 view;
  mat4 projection;
};

} // namespace core

#endif // !CORE_COMPONENTS_HPP

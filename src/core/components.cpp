#include "horizon/core/components.hpp"

#include "math/math.hpp"

namespace core {

void camera_t::update() {
  inv_projection = math::inverse(projection);
  inv_view       = math::inverse(view);
}
math::vec3 camera_t::position() const {
  return {inv_view[3][0], inv_view[3][1], inv_view[3][2]};
}

math::mat4 transform_t::mat4() const {
  return math::translate(math::mat4{1.f}, translation) *
         math::toMat4(math::quat{rotation}) *
         math::scale(math::mat4{1.f}, scale);
}

}  // namespace core

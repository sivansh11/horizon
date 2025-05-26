#include "horizon/core/components.hpp"
#include "math/math.hpp"

namespace core {

math::mat4 transform_t::mat4() const {
  return math::translate(math::mat4{1.f}, translation) *
         math::toMat4(math::quat{rotation}) *
         math::scale(math::mat4{1.f}, scale);
}

} // namespace core

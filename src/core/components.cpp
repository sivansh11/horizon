#include "horizon/core/components.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "horizon/core/math.hpp"

namespace core {

core::mat4 transform_t::mat4() const {
  return core::translate(core::mat4{1.f}, translation) *
         core::toMat4(quat{rotation}) * core::scale(core::mat4{1.f}, scale);
}

} // namespace core

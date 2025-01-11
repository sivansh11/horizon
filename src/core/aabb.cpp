#include "horizon/core/aabb.hpp"

#include "horizon/core/core.hpp"
#include "horizon/core/math.hpp"

namespace core {

bool aabb_t::is_valid() const {
    return min != vec3(infinity) && max != vec3(-infinity);
}

vec3 aabb_t::extent() const {
    check(is_valid(), "aabb is not valid!");
    return max - min;
}

float aabb_t::area() const {
    check(is_valid(), "aabb is not valid!");
    const vec3 e = extent();
    return 2.f * (e.x * e.y + e.y * e.z + e.z * e.x);
}

vec3 aabb_t::center() const {
    return (max + min) / 2.f;
}


aabb_t& aabb_t::grow(const vec3& point) {
    min = glm::min(min, point);
    max = glm::max(max, point);
    return *this;
}

aabb_t& aabb_t::grow(const aabb_t& aabb) {
    min = glm::min(min, aabb.min);
    max = glm::max(max, aabb.max);
    return *this;
}

} // namespace core

std::ostream& operator << (std::ostream& o, const core::aabb_t& aabb) {
  std::cout << "min:" << core::to_string(aabb.min) << " max:" << core::to_string(aabb.max);
  return o;
}

#ifndef AABB_HPP
#define AABB_HPP

#include "horizon/core/math.hpp"

#include <ostream>

namespace core {

struct aabb_t {
    bool    is_valid()  const;
    vec3    extent()    const;
    float   area()      const;
    vec3    center()    const;

    aabb_t& grow(const vec3& point);
    aabb_t& grow(const aabb_t& other);

    bool operator==(const aabb_t& other) const {
        return min == other.min && max == other.max;
    }

    vec3 min{  infinity };
    vec3 max{ -infinity };
};

} // namespace core

std::ostream& operator << (std::ostream& o, const core::aabb_t& aabb);

#endif

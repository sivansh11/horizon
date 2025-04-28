#ifndef TRIANGLE_HPP
#define TRIANGLE_HPP

#include "horizon/core/math.hpp"

#include "horizon/core/aabb.hpp"

namespace core {

struct triangle_t {
  aabb_t aabb() const { return aabb_t{}.grow(v0).grow(v1).grow(v2); }
  vec3 center() const { return (v0 + v1 + v2) / 3.f; }
  vec3 v0, v1, v2;
};

} // namespace core

#endif // !TRIANGLE_HPP

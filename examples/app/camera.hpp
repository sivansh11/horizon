#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "horizon/core/math.hpp"
#include "horizon/core/window.hpp"

class camera_t {
public:
  camera_t(core::window_t &window);

  void update(float ts);

  const core::mat4 &projection() const { return _projection; }
  const core::mat4 &view() const { return _view; }
  // creates a new mat4
  core::mat4 inverse_projection() const { return core::inverse(_projection); }
  // creates a new mat4
  core::mat4 inverse_view() const { return core::inverse(_view); }
  core::vec3 front() const { return _front; }
  core::vec3 right() const { return _right; }
  core::vec3 up() const { return _up; }
  core::vec3 position() const { return _position; }

  float fov{45.f};
  float camera_speed_multiplier{1};

  float far{1000.f};
  float near{0.1f};

private:
  core::window_t &_window;

  core::vec3 _position{0, 0, 0};
  core::mat4 _view{1.f};
  core::mat4 _projection{1.f};

  core::vec3 _front{0};
  core::vec3 _up{0, 1, 0};
  core::vec3 _right{0};

  core::vec2 _initial_mouse{};

  float _yaw{0};
  float _pitch{0};
  float _mouse_speed{0.005f};
  float _mouse_sensitivity{100.f};
};

#endif

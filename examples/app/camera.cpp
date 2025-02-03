#include "camera.hpp"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/trigonometric.hpp"

#include <GLFW/glfw3.h>

camera_t::camera_t(core::window_t &window) : _window(window) {}

void camera_t::update(float ts) {
  auto [width, height] = _window.dimensions();
  static float s_aspect_ratio = 0;
  float aspect_ratio = float(width) / float(height);
  if (s_aspect_ratio != aspect_ratio) {
    _projection =
        core::perspective(core::radians(fov), aspect_ratio, near, far);
    _projection[1][1] *= -1;
    s_aspect_ratio = aspect_ratio;
  }

  double curX, curY;
  glfwGetCursorPos(_window.window(), &curX, &curY);

  float velocity = _mouse_speed * ts;

  if (glfwGetKey(_window.window(), GLFW_KEY_W))
    _position += _front * velocity;
  if (glfwGetKey(_window.window(), GLFW_KEY_S))
    _position -= _front * velocity;
  if (glfwGetKey(_window.window(), GLFW_KEY_D))
    _position += _right * velocity;
  if (glfwGetKey(_window.window(), GLFW_KEY_A))
    _position -= _right * velocity;
  if (glfwGetKey(_window.window(), GLFW_KEY_SPACE))
    _position += _up * velocity;
  if (glfwGetKey(_window.window(), GLFW_KEY_LEFT_SHIFT))
    _position -= _up * velocity;

  core::vec2 mouse{curX, curY};
  core::vec2 difference = mouse - _initial_mouse;
  _initial_mouse = mouse;

  if (glfwGetMouseButton(_window.window(), GLFW_MOUSE_BUTTON_1)) {

    difference.x = difference.x / float(width);
    difference.y = -(difference.y / float(height));

    _yaw += difference.x * _mouse_sensitivity;
    _pitch += difference.y * _mouse_sensitivity;

    if (_pitch > 89.0f)
      _pitch = 89.0f;
    if (_pitch < -89.0f)
      _pitch = -89.0f;
  }

  core::vec3 front;
  front.x = core::cos(core::radians(_yaw)) * core::cos(core::radians(_pitch));
  front.y = core::sin(core::radians(_pitch));
  front.z = core::sin(core::radians(_yaw)) * core::cos(core::radians(_pitch));
  _front = front * camera_speed_multiplier;
  _right = core::normalize(core::cross(_front, core::vec3{0, 1, 0}));
  _up = core::normalize(core::cross(_right, _front));

  _view = core::lookAt(_position, _position + _front, core::vec3{0, 1, 0});
}

#include "horizon/core/core.hpp"
#include "horizon/core/logger.hpp"
#include "horizon/core/window.hpp"

#include "renderer.hpp"

#include <GLFW/glfw3.h>
#include <cassert>
#include <string>

class editor_camera_t {
public:
  editor_camera_t(core::window_t &window) : _window(window) {}
  void update_projection(float aspect_ratio);
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

public:
  float fov{90.0f}; // zoom var ?
  float camera_speed_multiplyer{1};

  float far{1000.f};
  float near{0.1f};

private:
  core::window_t &_window;

  core::vec3 _position{0, 2, 0};

  core::mat4 _projection{1.0f};
  core::mat4 _view{1.0f};

  core::vec3 _front{1, 0, 0};
  core::vec3 _up{0, 1, 0};
  core::vec3 _right{1, 0, 0};

  core::vec2 _initial_mouse{};

  float _yaw{0};
  float _pitch{0};
  float _mouse_speed{0.005f};
  float _mouse_sensitivity{100.f};
};

void editor_camera_t::update_projection(float aspect_ratio) {
  static float s_aspect_ratio = 0;
  if (s_aspect_ratio != aspect_ratio) {
    _projection =
        core::perspective(core::radians(fov), aspect_ratio, near, far);
    s_aspect_ratio = aspect_ratio;
  }
}

void editor_camera_t::update(float ts) {
  auto [width, height] = _window.dimensions();
  update_projection(float(width) / float(height));

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
  _front = front * camera_speed_multiplyer;
  _right = core::normalize(core::cross(_front, core::vec3{0, 1, 0}));
  _up = core::normalize(core::cross(_right, _front));

  _view = core::lookAt(_position, _position + _front, core::vec3{0, 1, 0});
}

int main(int argc, char **argv) {
  core::log_t::set_log_level(core::log_level_t::e_info);

  if (argc != 5) {
    std::cout << "./app [width] [height] [validation] [model file]\n";
    exit(EXIT_FAILURE);
  }
  //  core::log_t::set_log_level(core::log_level_t::e_info);

  core::window_t window{"app", (uint32_t)std::stoi(argv[1]), (uint32_t)std::stoi(argv[2])};
  auto [width, height] = window.dimensions();

  renderer::renderer_t renderer{window, (bool)std::stoi(argv[3]), std::filesystem::path(argv[4])};

  editor_camera_t editor_camera{window};
  float target_fps = 10000000.f;
  auto last_time = std::chrono::system_clock::now();
  core::frame_timer_t frame_timer{60.f};

  while (!window.should_close()) {
    core::clear_frame_function_times();
    core::window_t::poll_events();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE))
      break;

    auto current_time = std::chrono::system_clock::now();
    auto time_difference = current_time - last_time;
    if (time_difference.count() / 1e6 < 1000.f / target_fps) {
      continue;
    }
    last_time = current_time;
    auto dt = frame_timer.update();

    editor_camera.update(dt.count());
    // horizon_info("{} {}", core::to_string(editor_camera.position()),
    // core::to_string(editor_camera.front()));
    renderer::camera_t camera{};
    camera.inverse_view = editor_camera.inverse_view();
    camera.inverse_projection = editor_camera.inverse_projection();
    renderer.render_frame(camera);
  }

  return 0;
}

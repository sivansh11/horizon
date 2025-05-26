#ifndef CORE_WINDOW_HPP
#define CORE_WINDOW_HPP

#include <cstdint>
#include <string>

struct GLFWwindow;

namespace core {

class window_t {
public:
  static void poll_events();

  window_t(const std::string &title, uint32_t width, uint32_t height);
  ~window_t();

  std::string title() const { return _title; }
  GLFWwindow *window() const { return _p_window; }
  bool should_close() const;
  std::pair<int, int> dimensions() const;

  operator GLFWwindow *() const { return _p_window; }

  bool get_key_pressed(int key) const;

private:
  std::string _title;
  GLFWwindow *_p_window;
};

} // namespace core

#endif

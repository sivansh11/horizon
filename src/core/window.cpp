#include "horizon/core/window.hpp"

#include "horizon/core/core.hpp"
#include "horizon/core/logger.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace core {

struct glfw_initializer_t {
  glfw_initializer_t() {
    horizon_profile();
    if (!glfwInit()) {
      horizon_error("Failed to initialize glfw");
      const char *description;
      glfwGetError(&description);
      check(false, "{}", description);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  }

  ~glfw_initializer_t() {
    horizon_profile();
    glfwTerminate();
  }
};

static glfw_initializer_t glfw_initializer{};

void window_t::poll_events() {
  horizon_profile();
  glfwPollEvents();
}

window_t::window_t(const std::string &title, uint32_t width, uint32_t height)
    : _title(title) {
  horizon_profile();

  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  _p_window = glfwCreateWindow(width, height, _title.c_str(), NULL, NULL);
  const char *description;
  glfwGetError(&description);
  check(_p_window, "Failed to create window!\n{}", description);
  horizon_trace("created window");
}

window_t::~window_t() {
  horizon_profile();
  glfwDestroyWindow(_p_window);
  horizon_trace("destroyed window");
}

bool window_t::should_close() const {
  horizon_profile();
  return glfwWindowShouldClose(_p_window);
}

std::pair<int, int> window_t::dimensions() const {
  int width, height;
  glfwGetWindowSize(_p_window, &width, &height);
  return {width, height};
}
} // namespace core

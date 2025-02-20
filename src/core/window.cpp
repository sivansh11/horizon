#include "horizon/core/window.hpp"

#include "horizon/core/core.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace core {

struct glfw_initializer_t {
    glfw_initializer_t() {
        horizon_profile();
        if (!glfwInit()) {
            horizon_error("Failed to initialize glfw");
            exit(EXIT_FAILURE);
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

window_t::window_t(const std::string& title, uint32_t width, uint32_t height) {
    horizon_profile();

    // glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    _p_window = glfwCreateWindow(width, height, _title.c_str(), NULL, NULL);
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
    return { width, height };
}
} // namespace core

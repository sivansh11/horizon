#include "horizon/core/window.hpp"
#include <stdexcept>

#include "horizon/core/core.hpp"
#include "horizon/core/logger.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace core {

struct glfw_initializer_t {
  glfw_initializer_t() {
    horizon_profile();
    // glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
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

  // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

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

int key_to_glfw_key(key_t key) {
  switch (key) {
    case key_t::e_space: return GLFW_KEY_SPACE;
    case key_t::e_apostrophe: return GLFW_KEY_APOSTROPHE;
    case key_t::e_comma: return GLFW_KEY_COMMA;
    case key_t::e_minus: return GLFW_KEY_MINUS;
    case key_t::e_period: return GLFW_KEY_PERIOD;
    case key_t::e_slash: return GLFW_KEY_SLASH;
    case key_t::e_0: return GLFW_KEY_0;
    case key_t::e_1: return GLFW_KEY_1;
    case key_t::e_2: return GLFW_KEY_2;
    case key_t::e_3: return GLFW_KEY_3;
    case key_t::e_4: return GLFW_KEY_4;
    case key_t::e_5: return GLFW_KEY_5;
    case key_t::e_6: return GLFW_KEY_6;
    case key_t::e_7: return GLFW_KEY_7;
    case key_t::e_8: return GLFW_KEY_8;
    case key_t::e_9: return GLFW_KEY_9;
    case key_t::e_semicolon: return GLFW_KEY_SEMICOLON;
    case key_t::e_equal: return GLFW_KEY_EQUAL;
    case key_t::e_a: return GLFW_KEY_A;
    case key_t::e_b: return GLFW_KEY_B;
    case key_t::e_c: return GLFW_KEY_C;
    case key_t::e_d: return GLFW_KEY_D;
    case key_t::e_e: return GLFW_KEY_E;
    case key_t::e_f: return GLFW_KEY_F;
    case key_t::e_g: return GLFW_KEY_G;
    case key_t::e_h: return GLFW_KEY_H;
    case key_t::e_i: return GLFW_KEY_I;
    case key_t::e_j: return GLFW_KEY_J;
    case key_t::e_k: return GLFW_KEY_K;
    case key_t::e_l: return GLFW_KEY_L;
    case key_t::e_m: return GLFW_KEY_M;
    case key_t::e_n: return GLFW_KEY_N;
    case key_t::e_o: return GLFW_KEY_O;
    case key_t::e_p: return GLFW_KEY_P;
    case key_t::e_q: return GLFW_KEY_Q;
    case key_t::e_r: return GLFW_KEY_R;
    case key_t::e_s: return GLFW_KEY_S;
    case key_t::e_t: return GLFW_KEY_T;
    case key_t::e_u: return GLFW_KEY_U;
    case key_t::e_v: return GLFW_KEY_V;
    case key_t::e_w: return GLFW_KEY_W;
    case key_t::e_x: return GLFW_KEY_X;
    case key_t::e_y: return GLFW_KEY_Y;
    case key_t::e_z: return GLFW_KEY_Z;
    case key_t::e_left_bracket: return GLFW_KEY_LEFT_BRACKET;
    case key_t::e_backslash: return GLFW_KEY_BACKSLASH;
    case key_t::e_right_bracket: return GLFW_KEY_RIGHT_BRACKET;
    case key_t::e_grave_accent: return GLFW_KEY_GRAVE_ACCENT;
    case key_t::e_world_1: return GLFW_KEY_WORLD_1;
    case key_t::e_world_2: return GLFW_KEY_WORLD_2;

    case key_t::e_escape: return GLFW_KEY_ESCAPE;
    case key_t::e_enter: return GLFW_KEY_ENTER;
    case key_t::e_tab: return GLFW_KEY_TAB;
    case key_t::e_backspace: return GLFW_KEY_BACKSPACE;
    case key_t::e_insert: return GLFW_KEY_INSERT;
    case key_t::e_delete: return GLFW_KEY_DELETE;
    case key_t::e_right: return GLFW_KEY_RIGHT;
    case key_t::e_left: return GLFW_KEY_LEFT;
    case key_t::e_down: return GLFW_KEY_DOWN;
    case key_t::e_up: return GLFW_KEY_UP;
    case key_t::e_page_up: return GLFW_KEY_PAGE_UP;
    case key_t::e_page_down: return GLFW_KEY_PAGE_DOWN;
    case key_t::e_home: return GLFW_KEY_HOME;
    case key_t::e_end: return GLFW_KEY_END;
    case key_t::e_caps_lock: return GLFW_KEY_CAPS_LOCK;
    case key_t::e_scroll_lock: return GLFW_KEY_SCROLL_LOCK;
    case key_t::e_num_lock: return GLFW_KEY_NUM_LOCK;
    case key_t::e_print_screen: return GLFW_KEY_PRINT_SCREEN;
    case key_t::e_pause: return GLFW_KEY_PAUSE;
    case key_t::e_f1: return GLFW_KEY_F1;
    case key_t::e_f2: return GLFW_KEY_F2;
    case key_t::e_f3: return GLFW_KEY_F3;
    case key_t::e_f4: return GLFW_KEY_F4;
    case key_t::e_f5: return GLFW_KEY_F5;
    case key_t::e_f6: return GLFW_KEY_F6;
    case key_t::e_f7: return GLFW_KEY_F7;
    case key_t::e_f8: return GLFW_KEY_F8;
    case key_t::e_f9: return GLFW_KEY_F9;
    case key_t::e_f10: return GLFW_KEY_F10;
    case key_t::e_f11: return GLFW_KEY_F11;
    case key_t::e_f12: return GLFW_KEY_F12;
    case key_t::e_f13: return GLFW_KEY_F13;
    case key_t::e_f14: return GLFW_KEY_F14;
    case key_t::e_f15: return GLFW_KEY_F15;
    case key_t::e_f16: return GLFW_KEY_F16;
    case key_t::e_f17: return GLFW_KEY_F17;
    case key_t::e_f18: return GLFW_KEY_F18;
    case key_t::e_f19: return GLFW_KEY_F19;
    case key_t::e_f20: return GLFW_KEY_F20;
    case key_t::e_f21: return GLFW_KEY_F21;
    case key_t::e_f22: return GLFW_KEY_F22;
    case key_t::e_f23: return GLFW_KEY_F23;
    case key_t::e_f24: return GLFW_KEY_F24;
    case key_t::e_f25: return GLFW_KEY_F25;
    case key_t::e_kp_0: return GLFW_KEY_KP_0;
    case key_t::e_kp_1: return GLFW_KEY_KP_1;
    case key_t::e_kp_2: return GLFW_KEY_KP_2;
    case key_t::e_kp_3: return GLFW_KEY_KP_3;
    case key_t::e_kp_4: return GLFW_KEY_KP_4;
    case key_t::e_kp_5: return GLFW_KEY_KP_5;
    case key_t::e_kp_6: return GLFW_KEY_KP_6;
    case key_t::e_kp_7: return GLFW_KEY_KP_7;
    case key_t::e_kp_8: return GLFW_KEY_KP_8;
    case key_t::e_kp_9: return GLFW_KEY_KP_9;
    case key_t::e_kp_decimal: return GLFW_KEY_KP_DECIMAL;
    case key_t::e_kp_divide: return GLFW_KEY_KP_DIVIDE;
    case key_t::e_kp_multiply: return GLFW_KEY_KP_MULTIPLY;
    case key_t::e_kp_subtract: return GLFW_KEY_KP_SUBTRACT;
    case key_t::e_kp_add: return GLFW_KEY_KP_ADD;
    case key_t::e_kp_enter: return GLFW_KEY_KP_ENTER;
    case key_t::e_kp_equal: return GLFW_KEY_KP_EQUAL;
    case key_t::e_left_shift: return GLFW_KEY_LEFT_SHIFT;
    case key_t::e_left_control: return GLFW_KEY_LEFT_CONTROL;
    case key_t::e_left_alt: return GLFW_KEY_LEFT_ALT;
    case key_t::e_left_super: return GLFW_KEY_LEFT_SUPER;
    case key_t::e_right_shift: return GLFW_KEY_RIGHT_SHIFT;
    case key_t::e_right_control: return GLFW_KEY_RIGHT_CONTROL;
    case key_t::e_right_alt: return GLFW_KEY_RIGHT_ALT;
    case key_t::e_right_super: return GLFW_KEY_RIGHT_SUPER;
    case key_t::e_menu: return GLFW_KEY_MENU;
    default: return -1;
  }
  throw std::runtime_error("reached unreachable\n");
}

bool window_t::get_key_pressed(key_t key) const {
  return glfwGetKey(_p_window, key_to_glfw_key(key));
}

} // namespace core

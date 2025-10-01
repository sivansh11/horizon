#ifndef CORE_WINDOW_HPP
#define CORE_WINDOW_HPP

#include <cstdint>
#include <string>

struct GLFWwindow;

namespace core {

enum class key_t {
  e_space,
  e_apostrophe,
  e_comma,
  e_minus,
  e_period,
  e_slash,
  e_0,
  e_1,
  e_2,
  e_3,
  e_4,
  e_5,
  e_6,
  e_7,
  e_8,
  e_9,
  e_semicolon,
  e_equal,
  e_a,
  e_b,
  e_c,
  e_d,
  e_e,
  e_f,
  e_g,
  e_h,
  e_i,
  e_j,
  e_k,
  e_l,
  e_m,
  e_n,
  e_o,
  e_p,
  e_q,
  e_r,
  e_s,
  e_t,
  e_u,
  e_v,
  e_w,
  e_x,
  e_y,
  e_z,
  e_left_bracket,
  e_backslash,
  e_right_bracket,
  e_grave_accent,
  e_world_1,
  e_world_2,
  e_escape,
  e_enter,
  e_tab,
  e_backspace,
  e_insert,
  e_delete,
  e_right,
  e_left,
  e_down,
  e_up,
  e_page_up,
  e_page_down,
  e_home,
  e_end,
  e_caps_lock,
  e_scroll_lock,
  e_num_lock,
  e_print_screen,
  e_pause,
  e_f1,
  e_f2,
  e_f3,
  e_f4,
  e_f5,
  e_f6,
  e_f7,
  e_f8,
  e_f9,
  e_f10,
  e_f11,
  e_f12,
  e_f13,
  e_f14,
  e_f15,
  e_f16,
  e_f17,
  e_f18,
  e_f19,
  e_f20,
  e_f21,
  e_f22,
  e_f23,
  e_f24,
  e_f25,
  e_kp_0,
  e_kp_1,
  e_kp_2,
  e_kp_3,
  e_kp_4,
  e_kp_5,
  e_kp_6,
  e_kp_7,
  e_kp_8,
  e_kp_9,
  e_kp_decimal,
  e_kp_divide,
  e_kp_multiply,
  e_kp_subtract,
  e_kp_add,
  e_kp_enter,
  e_kp_equal,
  e_left_shift,
  e_left_control,
  e_left_alt,
  e_left_super,
  e_right_shift,
  e_right_control,
  e_right_alt,
  e_right_super,
  e_menu,
};

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

  bool get_key_pressed(key_t key) const;

private:
  std::string _title;
  GLFWwindow *_p_window;
};

} // namespace core

#endif

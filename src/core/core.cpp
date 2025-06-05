#include "horizon/core/core.hpp"

#include <fstream>

namespace core {

std::string read_file(const std::filesystem::path &filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);
  check(file.is_open(), "Failed to open file {}", filename.string());
  size_t file_size = static_cast<size_t>(file.tellg());
  std::string buffer;
  buffer.reserve(file_size);
  file.seekg(0);
  size_t counter = 0;
  while (counter++ != file_size)
    buffer += file.get();
  file.close();
  return buffer;
}

void write_file(const std::filesystem::path &filename, const void *data,
                size_t size) {
  std::ofstream file(filename, std::ios::binary);
  check(file.is_open(), "Failed to open file {}", filename.string());
  file.write(reinterpret_cast<const char *>(data), size);
  check(!file.bad(), "Failed to write to file {}", filename.string());
  file.close();
}

uint32_t number_of_bits_required_for_number(uint32_t n) {
  uint32_t r = 1;
  if (n >> 16) {
    r += 16;
    n >>= 16;
  }
  if (n >> 8) {
    r += 8;
    n >>= 8;
  }
  if (n >> 4) {
    r += 4;
    n >>= 4;
  }
  if (n >> 2) {
    r += 2;
    n >>= 2;
  }
  if (n - 1)
    ++r;
  return r;
}

float safe_inverse(float x) {
  static constexpr float epsilon = std::numeric_limits<float>::epsilon();
  if (std::abs(x) <= epsilon) {
    return x >= 0 ? 1.f / epsilon : -1.f / epsilon;
  }
  return 1.f / x;
}

float clamp(float val, float min, float max) {
  return val > max ? max : val < min ? min : val;
}

namespace timer {

static frame_function_times scope_total_time;

scope_timer_t::scope_timer_t(timer_end_callback_t timer_end_callback) noexcept {
  _timer_end_callback = timer_end_callback;
  _start = std::chrono::high_resolution_clock::now();
}

scope_timer_t::~scope_timer_t() noexcept {
  auto end = std::chrono::high_resolution_clock::now();
  duration_t duration = end - _start;
  _timer_end_callback(duration);
}

frame_function_timer_t::frame_function_timer_t(
    std::string_view scope_name) noexcept
    : _scope_timer([scope_name](duration_t duration) {
        auto itr = scope_total_time.find(scope_name);
        if (itr != scope_total_time.end()) {
          // found
          itr->second += duration;
          // itr->second.first += 1;
          // itr->second.second += duration;
        } else {
          // not found
          scope_total_time.emplace(std::pair{scope_name, duration});
          // scope_total_time.emplace(std::pair{scope_name, std::pair<uint32_t,
          // timer::duration_t>{1, duration}});
        }
      }) {}

} // namespace timer

void clear_frame_function_times() noexcept { timer::scope_total_time.clear(); }

frame_function_times &get_frame_function_times() {
  return timer::scope_total_time;
}

frame_timer_t::frame_timer_t(float target_fps) : _target_fps(target_fps) {
  _last_time = std::chrono::high_resolution_clock::now();
}

timer::duration_t frame_timer_t::update() {
  auto current_time = std::chrono::high_resolution_clock::now();
  auto delta_time =
      std::chrono::duration_cast<timer::duration_t>(current_time - _last_time);
  _last_time = current_time;
  return delta_time;
}

} // namespace core

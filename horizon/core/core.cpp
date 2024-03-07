#include "core.hpp"

namespace core {

namespace timer {

static std::unordered_map<std::string_view, duration_t> scope_total_time;

scope_timer_t::scope_timer_t(timer_end_callback_t timer_end_callback) noexcept {
    _timer_end_callback = timer_end_callback;
    _start = std::chrono::high_resolution_clock::now();
}

scope_timer_t::~scope_timer_t() noexcept {
    auto end = std::chrono::high_resolution_clock::now();
    duration_t duration = end - _start;
    _timer_end_callback(duration);
}

frame_function_timer_t::frame_function_timer_t(std::string_view scope_name) noexcept 
  : _scope_timer([scope_name](duration_t duration) {
        auto itr = scope_total_time.find(scope_name);
        if (itr != scope_total_time.end()) {
            // found
            itr->second += duration;
        } else {
            // not found 
            scope_total_time.emplace(std::pair{scope_name, duration});
        }
    })  
    { 
    
}

} // namespace timer

void clear_frame_function_times() noexcept {
    timer::scope_total_time.clear();
}

std::unordered_map<std::string_view, timer::duration_t>& get_frame_function_times() {
    return timer::scope_total_time;
}

} // namespace core

#ifndef CORE_CORE_HPP
#define CORE_CORE_HPP

#include "log.hpp"

#include <memory>
#include <chrono>
#include <unordered_map>
#include <functional>
#include <source_location>
#include <string_view>

namespace core {

template <typename type>
using ref = std::shared_ptr<type>;

template <typename type_t, typename... args_t>
static ref<type_t> make_ref(args_t&&... args) {
    return std::make_shared<type_t>(std::forward<args_t>(args)...);
}

// from: https://stackoverflow.com/a/57595105
template <typename type_t, typename... rest_t>
constexpr void hash_combine(uint64_t& seed, const type_t& v, const rest_t&... rest) {
    seed ^= std::hash<type_t>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    (hash_combine(seed, rest), ...);
};

namespace timer {

using duration_t = std::chrono::duration<double, std::milli>;

using timer_end_callback_t = std::function<void(duration_t)>;

struct scope_timer_t {
    scope_timer_t(timer_end_callback_t timer_end_callback) noexcept;
    ~scope_timer_t() noexcept;

    timer_end_callback_t _timer_end_callback;
    std::chrono::_V2::system_clock::time_point _start;
};

struct frame_function_timer_t {
    frame_function_timer_t(std::string_view scope_name) noexcept;

    scope_timer_t _scope_timer;
};

} // namespace timer

void clear_frame_function_times() noexcept;

std::unordered_map<std::string_view, timer::duration_t>& get_frame_function_times();

} // namespace core

template<>
struct fmt::formatter<core::timer::duration_t> {
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const core::timer::duration_t& input, FormatContext& ctx) -> decltype(ctx.out()) {
        return format_to(ctx.out(),
            "{}ms",
            input.count());
    }
};

#ifdef horizon_profile_enable
#define horizon_profile() core::timer::frame_function_timer_t frame_function_timer{std::source_location::current().function_name()}
#else
#define horizon_profile()
#endif

#define check(truthy, fail_msg)  \
do {                             \
    if (!(truthy)) {             \
        horizon_error(fail_msg); \
        std::terminate();        \
    }                            \
} while (false)

#endif
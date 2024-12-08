#ifndef CORE_CORE_HPP
#define CORE_CORE_HPP

#include "horizon/core/logger.hpp"

#include <memory>
#include <chrono>
#include <unordered_map>
#include <functional>
#include <filesystem>
#include <source_location>
#include <fstream>
#include <string_view>
#include <atomic>
#include <cstdint>

#ifdef horizon_profile_enable
#define horizon_profile() core::timer::frame_function_timer_t frame_function_timer{std::source_location::current().function_name()}
#else
#define horizon_profile()
#endif

#define check(truthy, fail_msg...)  \
do {                                \
    if (!(truthy)) {                \
        horizon_error(fail_msg);    \
        std::terminate();           \
    }                               \
} while (false)

#ifndef NDEBUG
#define horizon_assert(truthy, fail_msg...) \
check(truthy, fail_msg)
#else
#define horizon_assert(truthy, fail_msg...) 
#endif

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

struct binary_reader_t {
    binary_reader_t(const std::filesystem::path& path) : _path(path), _file(path, std::ios::binary) {
        check(_file.is_open(), "Failed to open file {}", _path.string());
    }

    ~binary_reader_t() {
        _file.close();
    }

    size_t file_size() {
        return std::filesystem::file_size(_path);
    }

    // TODO: add error handling
    template <typename type_t>
    void read(type_t& val) {
        _file.read(reinterpret_cast<char *>(&val), sizeof(type_t));
    }

    std::filesystem::path   _path;
    std::ifstream           _file;
};

struct binary_writer_t {
    binary_writer_t(const std::filesystem::path& path) : _path(path), _file(path, std::ios::binary) {
        check(_file.is_open(), "Failed to open file {}", _path.string());
    }

    ~binary_writer_t() {
        flush();
        _file.close();
    }

    template <typename type_t>
    void write(const type_t& val) {
        const char *data = reinterpret_cast<const char *>(&val);
        for (size_t i = 0; i < sizeof(type_t); i++) {
            _buffer.push_back(data[i]);
        }
    }

    void flush() {
        _file.write(_buffer.data(), _buffer.size());
        _buffer.clear();
    }

    std::filesystem::path   _path;
    std::ofstream           _file;
    std::vector<char>       _buffer;
};

std::string read_file(const std::filesystem::path& filename);
void write_file(const std::filesystem::path& filename, const void *data, size_t size);

uint32_t number_of_bits_required_for_number(uint32_t n);
float safe_inverse(float x);
float clamp(float val, float min, float max);


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

using frame_function_times = std::unordered_map<std::string_view, timer::duration_t>;

void clear_frame_function_times() noexcept;

frame_function_times& get_frame_function_times();

struct frame_timer_t {
    frame_timer_t(float target_fps);

    timer::duration_t update();

    float _target_fps;
    std::chrono::_V2::system_clock::time_point _last_time;
};

} // namespace core

template<>
struct std::formatter<core::timer::duration_t> {
    template <typename parse_context_t>
    constexpr auto parse(parse_context_t& ctx) {
        return ctx.begin();
    }

    template <typename format_context_t>
    auto format(const core::timer::duration_t& duration, format_context_t& ctx) const {
        return std::format_to(ctx.out(), "{}ms", duration);
    }
};

#endif

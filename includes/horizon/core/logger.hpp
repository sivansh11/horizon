#ifndef CORE_LOGGER_HPP
#define CORE_LOGGER_HPP

#include <iostream>
#include <format>

namespace core {

enum class log_level_t {
    e_trace = 0,
    e_info = 1,
    e_warn = 2,
    e_error = 3,
};


// TODO: make it thread safe
class log_t {
public:
    static void set_log_level(log_level_t level);

    static void set_trace_color(const std::string& str);
    static void set_info_color(const std::string& str);
    static void set_warn_color(const std::string& str);
    static void set_error_color(const std::string& str);

    static void log_trace(const std::string& str);
    static void log_info(const std::string& str);
    static void log_warn(const std::string& str);
    static void log_error(const std::string& str);

private:
    static log_level_t _log_level;
    static std::string _log_colors[4];
};

} // namespace core

#define horizon_trace(...) ::core::log_t::log_trace(std::format(__VA_ARGS__))
#define horizon_info(...) ::core::log_t::log_info(std::format(__VA_ARGS__))
#define horizon_warn(...) ::core::log_t::log_warn(std::format(__VA_ARGS__))
#define horizon_error(...) ::core::log_t::log_error(std::format(__VA_ARGS__))

#endif
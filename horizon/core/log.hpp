#ifndef CORE_LOG_HPP
#define CORE_LOG_HPP

#include <spdlog/spdlog.h>

#include <memory>

namespace spdlog {

class logger;

} // namespace spdlog

namespace core {

class log_t {
public:
    inline static std::shared_ptr<spdlog::logger>& get_logger() { return _logger; }

    friend class log_initializer_t;
private:
    static bool init();

    static std::shared_ptr<spdlog::logger> _logger;

};

} // namespace core

#define horizon_trace(...) ::core::log_t::get_logger()->trace(__VA_ARGS__)
#define horizon_info(...) ::core::log_t::get_logger()->info(__VA_ARGS__)
#define horizon_warn(...) ::core::log_t::get_logger()->warn(__VA_ARGS__)
#define horizon_error(...) ::core::log_t::get_logger()->error(__VA_ARGS__)


#endif
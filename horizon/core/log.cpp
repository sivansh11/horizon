#include "log.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <iostream>

namespace core {

std::shared_ptr<spdlog::logger> log_t::_logger;

struct log_initializer_t {
    log_initializer_t() {
        if (!log_t::init()) {
            std::cerr << "Failed to initialize logger\n";
            exit(EXIT_FAILURE);
        }
    }
};

static log_initializer_t log_initializer{};

bool log_t::init() {
    bool ok = false;
    std::vector<spdlog::sink_ptr> log_sink;
    log_sink.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    spdlog::set_pattern("%v");
    _logger = std::make_shared<spdlog::logger>("Horizon", begin(log_sink), end(log_sink));
    if (_logger) {
        ok = true;
        spdlog::register_logger(_logger);
        _logger->set_level(spdlog::level::trace);
        _logger->flush_on(spdlog::level::trace);
    }
    return ok;
}

} // namespace core

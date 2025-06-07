#include "horizon/core/logger.hpp"
#include <fstream>
#include <iostream>
#include <memory>

namespace core {

// TODO: make this thread safe

log_level_t log_t::_log_level = log_level_t::e_trace;
// https://gist.github.com/JBlond/2fea43a3049b38287e5e9cefc87b2124
std::string log_t::_log_colors[4] = {
    "\e[0;37m",
    "\e[0;92m",
    "\e[0;93m",
    "\e[0;91m",
};

static std::ostream *log_stream = &std::cout;
static std::unique_ptr<std::ofstream> log_file;

void log_t::set_log_file(const std::filesystem::path &path) {
  log_file = std::make_unique<std::ofstream>(path);
  log_stream = &(*log_file);
}

void log_t::set_log_level(log_level_t level) { _log_level = level; }

void log_t::set_trace_color(const std::string &str) { _log_colors[0] = str; }

void log_t::set_info_color(const std::string &str) { _log_colors[1] = str; }

void log_t::set_warn_color(const std::string &str) { _log_colors[2] = str; }

void log_t::set_error_color(const std::string &str) { _log_colors[3] = str; }

void log_t::log_trace(const std::string &str) {
  if (_log_level <= log_level_t::e_trace) {
    (*log_stream) << _log_colors[0] << str << "\e[0;97m" << '\n';
    log_stream->flush();
  }
}

void log_t::log_info(const std::string &str) {
  if (_log_level <= log_level_t::e_info) {
    (*log_stream) << _log_colors[1] << str << "\e[0;97m" << '\n';
    log_stream->flush();
  }
}

void log_t::log_warn(const std::string &str) {
  if (_log_level <= log_level_t::e_warn) {
    (*log_stream) << _log_colors[2] << str << "\e[0;97m" << '\n';
    log_stream->flush();
  }
}

void log_t::log_error(const std::string &str) {
  if (_log_level <= log_level_t::e_error) {
    (*log_stream) << _log_colors[3] << str << "\e[0;97m" << '\n';
    log_stream->flush();
  }
}

} // namespace core

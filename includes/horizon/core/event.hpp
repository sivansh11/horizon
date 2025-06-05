#ifndef CORE_EVENT_HPP
#define CORE_EVENT_HPP

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace core {

using event_id_t = uint32_t;

struct event_t {};

class dispatcher_t {
public:
  template <typename event_type_t>
  void subscribe(std::function<void(const event_t &)> callback) {
    event_id_t event_id = _get_event_id<event_type_t>();
    _callbacks[event_id].push_back(callback);
  }

  template <typename event_type_t, typename... args_t>
  void post(args_t &&...args) {
    event_type_t event{std::forward<args_t>(args)...};
    event_id_t event_id = _get_event_id<event_type_t>();
    auto itr = _callbacks.find(event_id);
    if (itr != _callbacks.end()) {
      for (auto &callback : itr->second) {
        callback(reinterpret_cast<event_t &>(event));
      }
    }
  }

private:
  template <typename event_type_t> event_id_t _get_event_id() {
    static event_id_t s_event_id = _event_id_t_counter++;
    return s_event_id;
  }

private:
  event_id_t _event_id_t_counter = 0;
  std::unordered_map<event_id_t,
                     std::vector<std::function<void(const event_t &)>>>
      _callbacks;
};

} // namespace core

#endif

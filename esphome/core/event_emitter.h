#pragma once
#include <map>
#include <vector>
#include <functional>
#include <string>
#include <algorithm>

namespace esphome {

using EventEmitterListenerID = uint32_t;

// EventEmitter class that can emit events with a specific name (usually an enum) and a list of arguments.
// Supports multiple listeners for each event.
template <typename EvtNames, typename... Args>
class EventEmitter {
 public:
  EventEmitterListenerID on(EvtNames event, std::function<void(Args...)> listener) {
    listeners_[event].emplace_back(++current_id_, [listener](Args... args) {
        listener(args...);
    });
    return current_id_;
}

  void off(EvtNames event, EventEmitterListenerID id) {
    if (this->listeners_.count(event) == 0)
        return;
    auto &vec = this->listeners_[event];
    vec.erase(std::remove_if(vec.begin(), vec.end(), [id](const std::pair<EventEmitterListenerID, std::function<void(Args...)>> &pair) {
        return pair.first == id;
    }), vec.end());
  }

 protected:
  void emit(EvtNames event, Args... args) {
    if (listeners_.count(event) == 0)
        return;
    for (const auto &listener : listeners_[event]) {
        listener.second(args...);
    }
  }

 private:
  std::map<EvtNames, std::vector<std::pair<EventEmitterListenerID, std::function<void(Args...)>>>> listeners_;
  EventEmitterListenerID current_id_ = 0;
};

}  // namespace esphome

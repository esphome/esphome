#pragma once
#include <unordered_map>
#include <vector>
#include <functional>

#include "esphome/core/log.h"

namespace esphome {

using EventEmitterListenerID = uint32_t;

// EventEmitter class that can emit events with a specific name (it is highly recommended to use an enum class for this) and a list of arguments.
// Supports multiple listeners for each event.
template <typename EvtType, typename... Args> class EventEmitter {
 public:
  EventEmitterListenerID on(EvtType event, std::function<void(Args...)> listener) {
    EventEmitterListenerID listener_id = get_next_id(event);
    listeners_[event][listener_id] = listener;
    return listener_id;
  }

  void off(EvtType event, EventEmitterListenerID id) {
    if (listeners_.count(event) == 0)
      return;
    listeners_[event].erase(id);
  }

 protected:
  void emit(EvtType event, Args... args) {
    if (listeners_.count(event) == 0)
      return;
    for (const auto &listener : listeners_[event]) {
      listener.second(args...);
    }
  }

  EventEmitterListenerID get_next_id(EvtType event) {
    // Check if the map is full
    if (listeners_[event].size() == std::numeric_limits<EventEmitterListenerID>::max()) {
      // Raise an error if the map is full
      ESP_LOGE("event_emitter", "EventEmitter has reached the maximum number of listeners for event %d", event);
      ESP_LOGW("event_emitter", "Removing listener with ID %d for event %d", 0, event);
      off(event, 0);
      return 0;
    }
    // Get the next ID for the given event.
    EventEmitterListenerID next_id = (current_id_ + 1) % std::numeric_limits<EventEmitterListenerID>::max();
    while (listeners_[event].count(next_id) > 0) {
      next_id = (next_id + 1) % std::numeric_limits<EventEmitterListenerID>::max();
    }
    current_id_ = next_id;
    return current_id_;
  }
  
 private:
  std::unordered_map<EvtType, std::unordered_map<EventEmitterListenerID, std::function<void(Args...)>>> listeners_;
  EventEmitterListenerID current_id_ = 0;
};

}  // namespace esphome

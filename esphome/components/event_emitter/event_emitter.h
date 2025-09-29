#pragma once
#include <array>
#include <functional>
#include <limits>

#include "esphome/core/log.h"

namespace esphome {
namespace event_emitter {

using EventEmitterListenerID = uint32_t;
static constexpr EventEmitterListenerID INVALID_LISTENER_ID = 0;

// EventEmitter class that can emit events with a specific name (it is highly recommended to use an enum class for this)
// and a list of arguments. Supports multiple listeners for each event.
// MaxListeners is the compile-time maximum number of listeners per event type.
template<typename EvtType, size_t MaxListeners, typename... Args> class EventEmitter {
 public:
  EventEmitterListenerID on(EvtType event, std::function<void(Args...)> listener) {
    // Find a free slot in the listeners array
    for (auto &entry : this->listeners_) {
      if (entry.id == INVALID_LISTENER_ID) {
        // Found empty slot
        EventEmitterListenerID listener_id = this->get_next_id_();
        entry.id = listener_id;
        entry.event = event;
        entry.callback = std::move(listener);
        return listener_id;
      }
    }
    // No free slots - array is full
    return INVALID_LISTENER_ID;
  }

  void off(EvtType event, EventEmitterListenerID id) {
    // Find and remove listener with given id
    for (auto &entry : this->listeners_) {
      if (entry.id == id && entry.event == event) {
        entry.id = INVALID_LISTENER_ID;
        entry.callback = nullptr;
        return;
      }
    }
  }

 protected:
  void emit_(EvtType event, Args... args) {
    // Call all listeners for this event
    for (const auto &entry : this->listeners_) {
      if (entry.id != INVALID_LISTENER_ID && entry.event == event) {
        entry.callback(args...);
      }
    }
  }

 private:
  struct ListenerEntry {
    EvtType event;
    EventEmitterListenerID id{INVALID_LISTENER_ID};
    std::function<void(Args...)> callback{nullptr};
  };

  EventEmitterListenerID get_next_id_() {
    // Simple incrementing ID, wrapping around at max
    EventEmitterListenerID next_id = (this->current_id_ + 1);
    if (next_id == INVALID_LISTENER_ID) {
      next_id = 1;
    }
    this->current_id_ = next_id;
    return this->current_id_;
  }

  std::array<ListenerEntry, MaxListeners> listeners_{};
  EventEmitterListenerID current_id_{0};
};

}  // namespace event_emitter
}  // namespace esphome

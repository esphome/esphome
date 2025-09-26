#pragma once
#include <vector>
#include <functional>
#include <limits>

#include "esphome/core/log.h"

namespace esphome {
namespace event_emitter {

using EventEmitterListenerID = uint32_t;
static constexpr EventEmitterListenerID INVALID_LISTENER_ID = 0;
void raise_event_emitter_full_error();

// EventEmitter class that can emit events with a specific name (it is highly recommended to use an enum class for this)
// and a list of arguments. Supports multiple listeners for each event.
template<typename EvtType, typename... Args> class EventEmitter {
 public:
  EventEmitterListenerID on(EvtType event, std::function<void(Args...)> listener) {
    EventEmitterListenerID listener_id = get_next_id_();

    // Find or create event entry
    EventEntry *entry = find_or_create_event_(event);
    entry->listeners.push_back({listener_id, listener});

    return listener_id;
  }

  void off(EvtType event, EventEmitterListenerID id) {
    EventEntry *entry = find_event_(event);
    if (entry == nullptr)
      return;

    // Remove listener with given id
    for (auto it = entry->listeners.begin(); it != entry->listeners.end(); ++it) {
      if (it->id == id) {
        // Swap with last and pop for efficient removal
        *it = entry->listeners.back();
        entry->listeners.pop_back();

        // Remove event entry if no more listeners
        if (entry->listeners.empty()) {
          remove_event_(event);
        }
        return;
      }
    }
  }

 protected:
  void emit_(EvtType event, Args... args) {
    EventEntry *entry = find_event_(event);
    if (entry == nullptr)
      return;

    // Call all listeners for this event
    for (const auto &listener : entry->listeners) {
      listener.callback(args...);
    }
  }

 private:
  struct Listener {
    EventEmitterListenerID id;
    std::function<void(Args...)> callback;
  };

  struct EventEntry {
    EvtType event;
    std::vector<Listener> listeners;
  };

  EventEntry *find_event_(EvtType event) {
    for (auto &entry : events_) {
      if (entry.event == event) {
        return &entry;
      }
    }
    return nullptr;
  }

  EventEntry *find_or_create_event_(EvtType event) {
    EventEntry *entry = find_event_(event);
    if (entry != nullptr)
      return entry;

    // Create new event entry
    events_.push_back({event, {}});
    return &events_.back();
  }

  void remove_event_(EvtType event) {
    for (auto it = events_.begin(); it != events_.end(); ++it) {
      if (it->event == event) {
        // Swap with last and pop
        *it = events_.back();
        events_.pop_back();
        return;
      }
    }
  }

  EventEmitterListenerID get_next_id_() {
    // Simple incrementing ID, wrapping around at max
    EventEmitterListenerID next_id = (current_id_ + 1);
    if (next_id == 0) {  // Skip 0 as it's often used as "invalid"
      next_id = 1;
    }
    current_id_ = next_id;
    return current_id_;
  }

  std::vector<EventEntry> events_;
  EventEmitterListenerID current_id_ = 0;
};

}  // namespace event_emitter
}  // namespace esphome

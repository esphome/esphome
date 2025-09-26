#pragma once
#include <vector>
#include <functional>
#include <limits>

#include "esphome/core/log.h"

namespace esphome {
namespace event_emitter {

using EventEmitterListenerID = uint32_t;
static constexpr EventEmitterListenerID INVALID_LISTENER_ID = 0;

// EventEmitter class that can emit events with a specific name (it is highly recommended to use an enum class for this)
// and a list of arguments. Supports multiple listeners for each event.
template<typename EvtType, typename... Args> class EventEmitter {
 public:
  EventEmitterListenerID on(EvtType event, std::function<void(Args...)> listener) {
    EventEmitterListenerID listener_id = this->get_next_id_();

    // Find or create event entry
    EventEntry *entry = this->find_or_create_event_(event);
    entry->listeners.push_back({listener_id, listener});

    return listener_id;
  }

  void off(EvtType event, EventEmitterListenerID id) {
    EventEntry *entry = this->find_event_(event);
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
          this->remove_event_(event);
        }
        return;
      }
    }
  }

 protected:
  void emit_(EvtType event, Args... args) {
    EventEntry *entry = this->find_event_(event);
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

  EventEmitterListenerID get_next_id_() {
    // Simple incrementing ID, wrapping around at max
    EventEmitterListenerID next_id = (this->current_id_ + 1);
    if (next_id == INVALID_LISTENER_ID) {
      next_id = 1;
    }
    this->current_id_ = next_id;
    return this->current_id_;
  }

  EventEntry *find_event_(EvtType event) {
    for (auto &entry : this->events_) {
      if (entry.event == event) {
        return &entry;
      }
    }
    return nullptr;
  }

  EventEntry *find_or_create_event_(EvtType event) {
    EventEntry *entry = this->find_event_(event);
    if (entry != nullptr)
      return entry;

    // Create new event entry
    this->events_.push_back({event, {}});
    return &this->events_.back();
  }

  void remove_event_(EvtType event) {
    for (auto it = this->events_.begin(); it != this->events_.end(); ++it) {
      if (it->event == event) {
        // Swap with last and pop
        *it = this->events_.back();
        this->events_.pop_back();
        return;
      }
    }
  }

  std::vector<EventEntry> events_;
  EventEmitterListenerID current_id_ = 0;
};

}  // namespace event_emitter
}  // namespace esphome

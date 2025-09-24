#pragma once

#include "esphome/components/event/event.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome {
namespace event {

template<typename... Ts> class TriggerEventAction : public Action<Ts...>, public Parented<Event> {
 public:
  TEMPLATABLE_VALUE(std::string, event_type)

  void play(Ts... x) override { this->parent_->trigger(this->event_type_.value(x...)); }
};

class EventTrigger : public Trigger<std::string> {
 public:
  EventTrigger(Event *event) {
    event->add_on_event_callback([this](const std::string &event_type) { this->trigger(event_type); });
  }
};

}  // namespace event
}  // namespace esphome

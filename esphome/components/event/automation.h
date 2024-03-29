#pragma once

#include "esphome/components/event/event.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome {
namespace event {

template<typename... Ts> class FireEventAction : public Action<Ts...> {
 public:
  explicit FireEventAction(Event *event) : event_(event) {}
  TEMPLATABLE_VALUE(std::string, event_type)

  void play(Ts... x) override { this->event_->fire_event(this->event_type_.value(x...)); }

 protected:
  Event *event_;
};

class EventTrigger : public Trigger<std::string> {
 public:
  EventTrigger(Event *event) {
    event->add_on_event_fired_callback([this](const std::string &event_type) { this->trigger(event_type); });
  }
};

}  // namespace event
}  // namespace esphome

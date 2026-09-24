#pragma once

#include "esphome/components/event/event.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome::event {

class EventTrigger final : public Trigger<StringRef> {
 public:
  EventTrigger(Event *event) {
    event->add_on_event_callback([this](StringRef event_type) { this->trigger(event_type); });
  }
};

}  // namespace esphome::event

#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "valve.h"

namespace esphome::valve {

class ValveOpenTrigger final : public Trigger<> {
 public:
  ValveOpenTrigger(Valve *a_valve) : valve_(a_valve) {
    a_valve->add_on_state_callback([this]() {
      if (this->valve_->is_fully_open()) {
        this->trigger();
      }
    });
  }

 protected:
  Valve *valve_;
};

class ValveClosedTrigger final : public Trigger<> {
 public:
  ValveClosedTrigger(Valve *a_valve) : valve_(a_valve) {
    a_valve->add_on_state_callback([this]() {
      if (this->valve_->is_fully_closed()) {
        this->trigger();
      }
    });
  }

 protected:
  Valve *valve_;
};

}  // namespace esphome::valve

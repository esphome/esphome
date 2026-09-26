#pragma once

#include "esphome/core/automation.h"
#include "climate.h"

namespace esphome::climate {

class ControlTrigger final : public Trigger<ClimateCall &> {
 public:
  ControlTrigger(Climate *climate) {
    climate->add_on_control_callback([this](ClimateCall &x) { this->trigger(x); });
  }
};

class StateTrigger final : public Trigger<Climate &> {
 public:
  StateTrigger(Climate *climate) {
    climate->add_on_state_callback([this](Climate &x) { this->trigger(x); });
  }
};

}  // namespace esphome::climate

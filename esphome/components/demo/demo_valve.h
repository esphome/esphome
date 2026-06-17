#pragma once

#include "esphome/components/valve/valve.h"

namespace esphome::demo {

enum class DemoValveType {
  TYPE_1,
  TYPE_2,
};

class DemoValve : public valve::Valve {
 public:
  valve::ValveTraits get_traits() override {
    valve::ValveTraits traits;
    if (this->type_ == DemoValveType::TYPE_2) {
      traits.set_supports_position(true);
      traits.set_supports_toggle(true);
      traits.set_supports_stop(true);
    }
    return traits;
  }

  void set_type(DemoValveType type) { this->type_ = type; }

 protected:
  void control(const valve::ValveCall &call) override {
    auto pos = call.get_position();
    if (pos.has_value()) {
      this->position = *pos;
      this->publish_state();
      return;
    }
    auto toggle = call.get_toggle();
    if (toggle.has_value()) {
      if (*toggle) {
        if (this->position == valve::VALVE_OPEN) {
          this->position = valve::VALVE_CLOSED;
          this->publish_state();
        } else {
          this->position = valve::VALVE_OPEN;
          this->publish_state();
        }
      }
      return;
    } else if (call.get_stop()) {
      this->current_operation = valve::VALVE_OPERATION_IDLE;
      this->publish_state();  // Keep the current position
      return;
    }
  }
  DemoValveType type_{};
};

}  // namespace esphome::demo

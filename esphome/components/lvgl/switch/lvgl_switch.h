#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace lvgl {

class LVGLSwitch : public switch_::Switch {
 public:
  void set_control_lambda(std::function<void(bool)> state_lambda) {
    this->state_lambda_ = state_lambda;
    if (this->initial_state_.has_value()) {
      this->state_lambda_(this->initial_state_.value());
      this->initial_state_.reset();
    }
  }

 protected:
  void write_state(bool value) override {
    if (this->state_lambda_ != nullptr)
      this->state_lambda_(value);
    else
      this->initial_state_ = value;
  }
  std::function<void(bool)> state_lambda_{};
  optional<bool> initial_state_{};
};

}  // namespace lvgl
}  // namespace esphome

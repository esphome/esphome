#pragma once

#include <utility>

#include "esphome/components/number/number.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace lvgl {

class LVGLNumber : public number::Number {
 public:
  void set_control_lambda(std::function<void(float)> control_lambda) {
    this->control_lambda_ = std::move(control_lambda);
    if (this->initial_state_.has_value()) {
      this->control_lambda_(this->initial_state_.value());
      this->initial_state_.reset();
    }
  }

 protected:
  void control(float value) override {
    if (this->control_lambda_ != nullptr) {
      this->control_lambda_(value);
    } else {
      this->initial_state_ = value;
    }
  }
  std::function<void(float)> control_lambda_{};
  optional<float> initial_state_{};
};

}  // namespace lvgl
}  // namespace esphome

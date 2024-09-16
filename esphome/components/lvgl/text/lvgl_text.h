#pragma once

#include <utility>

#include "esphome/components/text/text.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace lvgl {

class LVGLText : public text::Text {
 public:
  void set_control_lambda(std::function<void(const std::string)> control_lambda) {
    this->control_lambda_ = std::move(control_lambda);
    if (this->initial_state_.has_value()) {
      this->control_lambda_(this->initial_state_.value());
      this->initial_state_.reset();
    }
  }

 protected:
  void control(const std::string &value) override {
    if (this->control_lambda_ != nullptr) {
      this->control_lambda_(value);
    } else {
      this->initial_state_ = value;
    }
  }
  std::function<void(const std::string)> control_lambda_{};
  optional<std::string> initial_state_{};
};

}  // namespace lvgl
}  // namespace esphome

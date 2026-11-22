#pragma once

#include "esphome/components/text/text.h"

namespace esphome::lvgl {

class LVGLText final : public text::Text {
 public:
  // A setter, as the widget exists only after LVGL init; control() before then is buffered
  void set_control_lambda(void (*control_lambda)(const std::string &)) {
    this->control_lambda_ = control_lambda;
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
  void (*control_lambda_)(const std::string &){nullptr};
  optional<std::string> initial_state_{};
};

}  // namespace esphome::lvgl

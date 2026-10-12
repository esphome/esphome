#pragma once

#include "esphome/core/defines.h"
#ifdef USE_NUMBER

#include "ld2410s.h"
#include "esphome/components/number/number.h"

namespace esphome::ld2410s {

/// Edits one common parameter; scale converts the module's units to the number's.
class LD2410SNumber final : public number::Number, public Parented<LD2410SComponent> {
 public:
  LD2410SNumber(Parameter parameter, float scale) : parameter_(parameter), scale_(scale) {}

  void publish_raw(uint32_t raw) { this->publish_state(raw * this->scale_); }

 protected:
  void control(float value) override {
    if (this->parent_->set_parameter(this->parameter_, lroundf(value / this->scale_)))
      this->publish_state(value);
  }

  const Parameter parameter_;
  const float scale_;
};

}  // namespace esphome::ld2410s

#endif  // USE_NUMBER

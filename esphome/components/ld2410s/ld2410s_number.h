#pragma once

#include "esphome/core/defines.h"
#if defined(USE_NUMBER) && defined(USE_LD2410S_PARAMETERS)

#include "ld2410s.h"
#include "esphome/components/number/number.h"

namespace esphome::ld2410s {

/// Edits one common parameter; scale converts the module's units to the number's.
class LD2410SNumber final : public number::Number, public Parented<LD2410SComponent> {
 public:
  LD2410SNumber(Parameter parameter, float scale) : parameter_(parameter), scale_(scale) {}

  void publish_raw(uint8_t raw) { this->publish_state(raw * this->scale_); }

 protected:
  void control(float value) override {
    // Show the step the module got, not a value between two steps
    const uint8_t raw = lroundf(value / this->scale_);  // the schema keeps every parameter within a byte
    if (this->parent_->set_parameter(this->parameter_, raw))
      this->publish_raw(raw);
  }

  const Parameter parameter_;
  const float scale_;
};

}  // namespace esphome::ld2410s

#endif  // USE_NUMBER && USE_LD2410S_PARAMETERS

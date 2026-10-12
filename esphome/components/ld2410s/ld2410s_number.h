#pragma once

#include "esphome/core/defines.h"
#ifdef USE_NUMBER

#include "ld2410s.h"
#include "esphome/components/number/number.h"

namespace esphome::ld2410s {

/// Edits one common parameter; the value is converted to the module's units by the hub.
class LD2410SNumber final : public number::Number, public Parented<LD2410SComponent> {
 public:
  explicit LD2410SNumber(Parameter parameter) : parameter_(parameter) {}

 protected:
  void control(float value) override {
    uint32_t raw;
    switch (this->parameter_) {
      case Parameter::MAX_DISTANCE:
      case Parameter::MIN_DISTANCE:
        raw = lroundf(value / GATE_WIDTH_M);  // the module counts 0.7 m gates
        break;
      case Parameter::STATUS_FREQUENCY:
      case Parameter::DISTANCE_FREQUENCY:
        raw = lroundf(value * 10);  // tenths of a hertz
        break;
      default:
        raw = lroundf(value);
        break;
    }
    if (this->parent_->set_parameter(this->parameter_, raw))
      this->publish_state(value);
  }

  const Parameter parameter_;
};

}  // namespace esphome::ld2410s

#endif  // USE_NUMBER

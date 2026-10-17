#include "m5_unit_bldc_number.h"

namespace esphome::m5_unit_bldc {

void M5UnitBldcNumber::control(float value) {
  this->publish_state(value);
  switch (this->type_) {
    case NumberType::PWM:
      this->parent_->write_pwm(static_cast<uint16_t>(value));
      break;
    case NumberType::TARGET_RPM:
      this->parent_->write_target_rpm(value);
      break;
  }
}

}  // namespace esphome::m5_unit_bldc

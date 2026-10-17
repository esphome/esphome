#include "m5_unit_bldc_number.h"

namespace esphome::m5_unit_bldc {

void M5UnitBldcNumber::control(float value) {
  bool success;
  switch (this->type_) {
    case NumberType::NUMBER_TYPE_PWM:
      success = this->parent_->write_pwm(static_cast<uint16_t>(value));
      break;
    case NumberType::NUMBER_TYPE_TARGET_RPM:
      success = this->parent_->write_target_rpm(value);
      break;
    default:
      success = false;
      break;
  }
  if (success)
    this->publish_state(value);
}

}  // namespace esphome::m5_unit_bldc

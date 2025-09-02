#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "qmi8658.h"

namespace esphome {
namespace qmi8658 {

template<typename... Ts> class EnableWakeOnMotionAction : public Action<Ts...>, public Parented<QMI8658Component> {
 public:
  TEMPLATABLE_VALUE(QMI8658AccelODR, accel_odr)
  TEMPLATABLE_VALUE(QMI8658AccelRange, accel_range)
  TEMPLATABLE_VALUE(uint8_t, blanking_time)
  TEMPLATABLE_VALUE(uint8_t, initial_pin_state)
  TEMPLATABLE_VALUE(QMI8658InterruptPin, interrupt_pin)
  TEMPLATABLE_VALUE(uint8_t, threshold)

  void play(Ts... x) override {
    QMI8658AccelRange accel_range = this->accel_range_.value(x...);
    QMI8658AccelODR accel_odr = this->accel_odr_.value(x...);
    uint8_t blanking_time = this->blanking_time_.value(x...);
    uint8_t initial_pin_state = this->initial_pin_state_.value(x...);
    QMI8658InterruptPin interrupt_pin = this->interrupt_pin_.value(x...);
    uint8_t threshold = this->threshold_.value(x...);
    this->parent_->enable_wake_on_motion(threshold, accel_range, accel_odr, interrupt_pin, initial_pin_state, blanking_time);
  }
};

}  // namespace qmi8658
}  // namespace esphome

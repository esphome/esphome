#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/tmc2209/tmc2209.h"
#include "esphome/core/component.h"

namespace esphome::tmc2209 {

class TMC2209Sensor : public PollingComponent {
 public:
  explicit TMC2209Sensor(TMC2209Stepper *parent) : parent_(parent) {}

  void update() override;
  void dump_config() override;

  SUB_SENSOR(actual_current)
  SUB_SENSOR(stallguard_result)
  SUB_SENSOR(motor_load)
  SUB_SENSOR(pwm_scale_sum)
  SUB_SENSOR(pwm_scale_auto)
  SUB_SENSOR(pwm_ofs_auto)
  SUB_SENSOR(pwm_grad_auto)

 protected:
  TMC2209Stepper *parent_;
};

}  // namespace esphome::tmc2209

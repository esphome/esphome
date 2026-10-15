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

  void set_actual_current_sensor(sensor::Sensor *sensor) { this->actual_current_sensor_ = sensor; }
  void set_stallguard_result_sensor(sensor::Sensor *sensor) { this->stallguard_result_sensor_ = sensor; }
  void set_motor_load_sensor(sensor::Sensor *sensor) { this->motor_load_sensor_ = sensor; }
  void set_pwm_scale_sum_sensor(sensor::Sensor *sensor) { this->pwm_scale_sum_sensor_ = sensor; }
  void set_pwm_scale_auto_sensor(sensor::Sensor *sensor) { this->pwm_scale_auto_sensor_ = sensor; }
  void set_pwm_ofs_auto_sensor(sensor::Sensor *sensor) { this->pwm_ofs_auto_sensor_ = sensor; }
  void set_pwm_grad_auto_sensor(sensor::Sensor *sensor) { this->pwm_grad_auto_sensor_ = sensor; }

 protected:
  TMC2209Stepper *parent_;
  sensor::Sensor *actual_current_sensor_{nullptr};
  sensor::Sensor *stallguard_result_sensor_{nullptr};
  sensor::Sensor *motor_load_sensor_{nullptr};
  sensor::Sensor *pwm_scale_sum_sensor_{nullptr};
  sensor::Sensor *pwm_scale_auto_sensor_{nullptr};
  sensor::Sensor *pwm_ofs_auto_sensor_{nullptr};
  sensor::Sensor *pwm_grad_auto_sensor_{nullptr};
};

}  // namespace esphome::tmc2209

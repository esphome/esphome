#pragma once

#include "esphome/components/pm100x/pm100x.h"

namespace esphome::pm100x_pwm {

// PWM-enabled subclass
class PM100XComponentPWM : public pm100x::PM100XComponent {
 public:
  void set_pwm_sensor(sensor::Sensor *pwm_sensor) { this->pwm_sensor_ = pwm_sensor; }
  void setup() override;
  void dump_config() override;
  void update() override {}

 protected:
  void handle_pwm_state_(float duty_percent);

  sensor::Sensor *pwm_sensor_{nullptr};
};

}  // namespace esphome::pm100x_pwm

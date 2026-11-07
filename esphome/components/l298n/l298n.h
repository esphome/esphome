#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/stepper/stepper.h"

namespace esphome::l298n {

enum L298NStepMode {
  L298N_STEP_MODE_FULL_STEP,
  L298N_STEP_MODE_HALF_STEP,
  L298N_STEP_MODE_WAVE_DRIVE,
};

class L298N final : public stepper::Stepper, public Component {
 public:
  void set_pin_a(GPIOPin *pin_a) { pin_a_ = pin_a; }
  void set_pin_b(GPIOPin *pin_b) { pin_b_ = pin_b; }
  void set_pin_c(GPIOPin *pin_c) { pin_c_ = pin_c; }
  void set_pin_d(GPIOPin *pin_d) { pin_d_ = pin_d; }
  void set_enable_pin(GPIOPin *enable_pin_a) { this->enable_pin_a = enable_pin_a; }
  void set_enable_pin(GPIOPin *enable_pin_b) { this->enable_pin_b = enable_pin_b; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void set_sleep_when_done(bool sleep_when_done) { this->sleep_when_done_ = sleep_when_done; }
  void set_step_mode(L298NStepMode step_mode) { this->step_mode_ = step_mode; }

 protected:
  void write_step_(int32_t step);

  bool sleep_when_done_{false};
  GPIOPin *pin_a_;
  GPIOPin *pin_b_;
  GPIOPin *pin_c_;
  GPIOPin *pin_d_;
  GPIOPin *enable_pin_a;
  GPIOPin *enable_pin_b;
  L298NStepMode step_mode_{L298N_STEP_MODE_FULL_STEP};
  HighFrequencyLoopRequester high_freq_;
  int32_t current_pos_{0};
};

}  // namespace esphome::l298n

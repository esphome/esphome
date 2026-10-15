#pragma once

#include "esphome/components/m5pm1/const.h"
#include "esphome/components/output/float_output.h"
#include "../m5pm1.h"
#include "esphome/core/helpers.h"

namespace esphome::m5pm1 {

class M5PM1FloatOutput : public output::FloatOutput, public Component, public Parented<M5PM1Component> {
 public:
  void setup() override;
  void dump_config() override;
  void set_channel(M5PM1PWMChannel channel) { this->channel_ = channel; }
  void set_pin(M5PM1GPIOPin *pin) { this->pin_ = pin; }
  void set_pwm_polarity(M5PM1PWMPolarity polar) { this->polar_ = polar; }

 protected:
  void write_state(float state) override;
  M5PM1GPIOPin *pin_{nullptr};
  M5PM1PWMChannel channel_{0};
  M5PM1PWMPolarity polar_{M5PM1PWMPolarity::M5PM1_PWM_POLARITY_ACTIVE_HIGH};
};

}  // namespace esphome::m5pm1

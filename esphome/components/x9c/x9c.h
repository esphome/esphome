#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace x9c {

class X9cOutput : public output::FloatOutput, public Component {
 public:
  void set_cs_pin(InternalGPIOPin *pin) { cs_pin_ = pin; }
  void set_inc_pin(InternalGPIOPin *pin) { inc_pin_ = pin; }
  void set_ud_pin(InternalGPIOPin *pin) { ud_pin_ = pin; }
  void set_initial_value(float initial_value) { initial_value_ = initial_value; }

  void setup() override;
  void dump_config() override;

  void trim_value(int change_amount);

 protected:
  void write_state(float state) override;
  InternalGPIOPin *cs_pin_;
  InternalGPIOPin *inc_pin_;
  InternalGPIOPin *ud_pin_;
  float initial_value_;
  float pot_value_;
};

}  // namespace x9c
}  // namespace esphome

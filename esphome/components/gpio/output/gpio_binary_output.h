#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/output/binary_output.h"

namespace esphome::gpio {

class GPIOBinaryOutput final : public output::BinaryOutput, public Component {
 public:
  void set_pin(GPIOPin *pin) { pin_ = pin; }

  void setup() override {
    if (pin_state_held_from_deep_sleep(this->pin_)) {
      // keep state and don't turn off
      this->pin_->setup();
    } else {
      this->turn_off();
      this->pin_->setup();
      this->turn_off();
    }
  }
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void write_state(bool state) override { this->pin_->digital_write(state); }

  GPIOPin *pin_;
};

}  // namespace esphome::gpio

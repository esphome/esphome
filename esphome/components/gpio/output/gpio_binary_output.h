#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/output/binary_output.h"

namespace esphome::gpio {

class GPIOBinaryOutput final : public output::BinaryOutput, public Component {
 public:
  void set_pin(GPIOPin *pin) { pin_ = pin; }

  void setup() override {
    // Only configure the pin mode here. Do not force the pin off: LightState (and
    // similar consumers) restore the intended level shortly after setup. Forcing
    // OFF first causes a visible off→on flash when the previous state was ON
    // (especially with PCF857x expanders that keep latch state across ESP reset).
    // See https://github.com/esphome/issues/issues/5390
    this->pin_->setup();
  }
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void write_state(bool state) override { this->pin_->digital_write(state); }

  GPIOPin *pin_;
};

}  // namespace esphome::gpio

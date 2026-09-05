#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/output/binary_output.h"
#if defined(USE_DEEP_SLEEP) && defined(USE_GPIO_HOLD)
#include "esphome/components/deep_sleep/deep_sleep_component.h"
#endif

namespace esphome::gpio {

class GPIOBinaryOutput final : public output::BinaryOutput, public Component {
 public:
  void set_pin(GPIOPin *pin) { pin_ = pin; }

  void setup() override {
#if defined(USE_DEEP_SLEEP) && defined(USE_GPIO_HOLD)
    if ((this->pin_->get_flags() & gpio::FLAG_HOLD) && deep_sleep::woken_from_deepsleep()) {
      // keep state and don't turn off
      this->pin_->setup();
    } else
#endif
    {
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

#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome::sm16716 {

class SM16716 final : public Component {
 public:
  /// @param pwm_amounts storage for num_channels duty values, owned by the caller for the life of the program.
  SM16716(GPIOPin *data_pin, GPIOPin *clock_pin, uint8_t *pwm_amounts, uint8_t num_channels)
      : data_pin_(data_pin), clock_pin_(clock_pin), pwm_amounts_(pwm_amounts), num_channels_(num_channels) {}

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  /// Send the new duty values, then sleep until write_channel() changes one.
  void loop() override;

  void write_channel(uint8_t channel, float state) {
    auto value = uint8_t(state * 0xFF);
    uint8_t index = this->num_channels_ - channel - 1;
    if (this->pwm_amounts_[index] != value) {
      this->pwm_amounts_[index] = value;
      this->enable_loop();
    }
  }

 protected:
  void write_bit_(bool value) {
    this->data_pin_->digital_write(value);
    this->clock_pin_->digital_write(true);
    this->clock_pin_->digital_write(false);
  }
  void write_byte_(uint8_t data) {
    for (uint8_t mask = 0x80; mask; mask >>= 1) {
      this->write_bit_(data & mask);
    }
  }

  GPIOPin *const data_pin_;
  GPIOPin *const clock_pin_;
  uint8_t *const pwm_amounts_;
  const uint8_t num_channels_;
};

}  // namespace esphome::sm16716

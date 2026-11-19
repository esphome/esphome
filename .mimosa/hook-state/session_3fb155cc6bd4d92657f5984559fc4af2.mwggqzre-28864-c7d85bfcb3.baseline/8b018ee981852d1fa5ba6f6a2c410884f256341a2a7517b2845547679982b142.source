#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include <span>

namespace esphome::sm16716 {

class SM16716 final : public Component {
 public:
  /// @param pwm_amounts one duty value per channel, a static buffer from code generation.
  SM16716(GPIOPin *data_pin, GPIOPin *clock_pin, std::span<uint8_t> pwm_amounts)
      : data_pin_(data_pin), clock_pin_(clock_pin), pwm_amounts_(pwm_amounts) {}

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  /// Send the new duty values, then sleep until write_channel() changes one.
  void loop() override;

  void write_channel(uint8_t channel, float state) {
    auto value = uint8_t(state * 0xFF);
    uint8_t index = this->pwm_amounts_.size() - channel - 1;
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
  const std::span<uint8_t> pwm_amounts_;
};

}  // namespace esphome::sm16716

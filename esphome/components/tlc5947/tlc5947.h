#pragma once
// TLC5947 24-Channel, 12-Bit PWM LED Driver
// https://www.ti.com/lit/ds/symlink/tlc5947.pdf

#include "esphome/core/component.h"
#include "esphome/core/hal.h"

#include <span>

namespace esphome::tlc5947 {

class TLC5947 final : public Component {
 public:
  // Must match CHANNELS_PER_CHIP in __init__.py, which validates the channel range
  static constexpr uint8_t N_CHANNELS_PER_CHIP = 24;

  /// @param pwm_amounts N_CHANNELS_PER_CHIP duty values per chip, a static buffer from code generation.
  TLC5947(GPIOPin *data_pin, GPIOPin *clock_pin, GPIOPin *lat_pin, std::span<uint16_t> pwm_amounts)
      : data_pin_(data_pin), clock_pin_(clock_pin), lat_pin_(lat_pin), pwm_amounts_(pwm_amounts) {}

  void set_outenable_pin(GPIOPin *outenable_pin) { this->outenable_pin_ = outenable_pin; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  /// Send the new duty values, then sleep until set_channel_value() changes one.
  void loop() override;

  void set_channel_value(uint16_t channel, uint16_t value);
  /// Set a channel from an output level, 0.0 to 1.0.
  void write_channel(uint16_t channel, float state) {
    this->set_channel_value(channel, static_cast<uint16_t>(state * 0xfff));
  }

 protected:
  GPIOPin *const data_pin_;
  GPIOPin *const clock_pin_;
  GPIOPin *const lat_pin_;
  GPIOPin *outenable_pin_{nullptr};
  const std::span<uint16_t> pwm_amounts_;
};

}  // namespace esphome::tlc5947

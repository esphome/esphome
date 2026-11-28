#pragma once
// TLC5971 12-Channel, 16-Bit PWM LED Driver
// https://www.ti.com/lit/ds/symlink/tlc5971.pdf

#include "esphome/core/component.h"
#include "esphome/core/hal.h"

#include <span>

namespace esphome::tlc5971 {

class TLC5971 final : public Component {
 public:
  // Must match CHANNELS_PER_CHIP in __init__.py, which validates the channel range
  static constexpr uint8_t N_CHANNELS_PER_CHIP = 12;

  /// @param pwm_amounts N_CHANNELS_PER_CHIP duty values per chip, a static buffer from code generation.
  TLC5971(GPIOPin *data_pin, GPIOPin *clock_pin, std::span<uint16_t> pwm_amounts)
      : data_pin_(data_pin), clock_pin_(clock_pin), pwm_amounts_(pwm_amounts) {}

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  /// Send the new duty values, then sleep until set_channel_value() changes one.
  void loop() override;

  void set_channel_value(uint16_t channel, uint16_t value);
  /// Set a channel from an output level, 0.0 to 1.0.
  void write_channel(uint16_t channel, float state) {
    this->set_channel_value(channel, static_cast<uint16_t>(state * 0xffff));
  }

 protected:
  void transfer_(uint8_t send);

  GPIOPin *const data_pin_;
  GPIOPin *const clock_pin_;
  const std::span<uint16_t> pwm_amounts_;
};

}  // namespace esphome::tlc5971

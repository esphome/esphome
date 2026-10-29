#pragma once
// TLC5947 24-Channel, 12-Bit PWM LED Driver
// https://www.ti.com/lit/ds/symlink/tlc5947.pdf

#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome::tlc5947 {

class TLC5947 final : public Component {
 public:
  static constexpr uint8_t N_CHANNELS_PER_CHIP = 24;

  /// @param pwm_amounts storage for num_chips * N_CHANNELS_PER_CHIP duty values, owned by the caller for the life
  /// of the program.
  TLC5947(GPIOPin *data_pin, GPIOPin *clock_pin, GPIOPin *lat_pin, uint16_t *pwm_amounts, uint8_t num_chips)
      : data_pin_(data_pin),
        clock_pin_(clock_pin),
        lat_pin_(lat_pin),
        pwm_amounts_(pwm_amounts),
        num_chips_(num_chips) {}

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
  uint16_t *const pwm_amounts_;
  const uint8_t num_chips_;
};

}  // namespace esphome::tlc5947

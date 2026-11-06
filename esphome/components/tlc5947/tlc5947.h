#pragma once
// TLC5947 24-Channel, 12-Bit PWM LED Driver
// https://www.ti.com/lit/ds/symlink/tlc5947.pdf

#include "esphome/core/component.h"
#include "esphome/core/hal.h"

#include <span>

namespace esphome::tlc5947 {

class TLC5947 final : public Component {
 public:
  static constexpr uint8_t N_CHANNELS_PER_CHIP = 24;

  /// `pwm_amounts` holds N_CHANNELS_PER_CHIP values per chip, a static array from code generation
  explicit TLC5947(std::span<uint16_t> pwm_amounts) : pwm_amounts_(pwm_amounts) {}

  void set_data_pin(GPIOPin *data_pin) { data_pin_ = data_pin; }
  void set_clock_pin(GPIOPin *clock_pin) { clock_pin_ = clock_pin; }
  void set_lat_pin(GPIOPin *lat_pin) { lat_pin_ = lat_pin; }
  void set_outenable_pin(GPIOPin *outenable_pin) { outenable_pin_ = outenable_pin; }

  void setup() override;

  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  /// Send new values if they were updated.
  void loop() override;

  void set_channel_value(uint16_t channel, uint16_t value);
  /// Set a channel from an output level, 0.0 to 1.0.
  void write_channel(uint16_t channel, float state) {
    this->set_channel_value(channel, static_cast<uint16_t>(state * 0xfff));
  }

 protected:
  GPIOPin *data_pin_;
  GPIOPin *clock_pin_;
  GPIOPin *lat_pin_;
  GPIOPin *outenable_pin_{nullptr};
  std::span<uint16_t> pwm_amounts_;
  bool update_{true};
};

}  // namespace esphome::tlc5947

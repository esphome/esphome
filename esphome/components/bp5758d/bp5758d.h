#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include <array>

namespace esphome::bp5758d {

class BP5758D final : public Component {
 public:
  BP5758D(GPIOPin *data_pin, GPIOPin *clock_pin) : data_pin_(data_pin), clock_pin_(clock_pin) {}

  void setup() override;

  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  /// Send the new duty values, then sleep until write_channel() changes one.
  void loop() override;

  /// @param channel 0-based, OUT1 is channel 0.
  void write_channel(uint8_t channel, float state);
  /// @param channel 0-based, OUT1 is channel 0.
  /// @param current the register value, already remapped by codegen.
  void set_channel_current(uint8_t channel, uint8_t current) { this->channel_current_[channel] = current; }

 protected:
  void write_bit_(bool value);
  void write_byte_(uint8_t data);
  void write_buffer_(const uint8_t *buffer, uint8_t size);

  GPIOPin *const data_pin_;
  GPIOPin *const clock_pin_;
  std::array<uint16_t, 5> pwm_amounts_{};
  std::array<uint8_t, 5> channel_current_{};
};

}  // namespace esphome::bp5758d

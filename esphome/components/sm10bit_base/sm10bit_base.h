#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include <array>

namespace esphome::sm10bit_base {

class Sm10BitBase : public Component {
 public:
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void setup() override;
  /// Send the new duty values, then sleep until write_channel() changes one.
  void loop() override;

  void write_channel(uint8_t channel, float state);

 protected:
  Sm10BitBase(GPIOPin *data_pin, GPIOPin *clock_pin, uint8_t model_id, uint8_t max_power_color_channels,
              uint8_t max_power_white_channels)
      : data_pin_(data_pin),
        clock_pin_(clock_pin),
        model_id_(model_id),
        max_power_color_channels_(max_power_color_channels),
        max_power_white_channels_(max_power_white_channels) {}

  void write_bit_(bool value);
  void write_byte_(uint8_t data);
  void write_buffer_(const uint8_t *buffer, uint8_t size);

  GPIOPin *const data_pin_;
  GPIOPin *const clock_pin_;
  std::array<uint16_t, 5> pwm_amounts_{};
  const uint8_t model_id_;
  const uint8_t max_power_color_channels_;
  const uint8_t max_power_white_channels_;
};

}  // namespace esphome::sm10bit_base

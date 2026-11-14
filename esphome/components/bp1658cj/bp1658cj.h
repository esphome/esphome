#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include <array>

namespace esphome::bp1658cj {

class BP1658CJ final : public Component {
 public:
  BP1658CJ(GPIOPin *data_pin, GPIOPin *clock_pin, uint8_t max_power_color_channels, uint8_t max_power_white_channels)
      : data_pin_(data_pin),
        clock_pin_(clock_pin),
        max_power_color_channels_(max_power_color_channels),
        max_power_white_channels_(max_power_white_channels) {}

  void setup() override;

  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  /// Send the new duty values, then sleep until write_channel() changes one.
  void loop() override;

  void write_channel(uint8_t channel, float state);

 protected:
  void write_bit_(bool value);
  void write_byte_(uint8_t data);
  void write_buffer_(const uint8_t *buffer, uint8_t size);

  GPIOPin *const data_pin_;
  GPIOPin *const clock_pin_;
  std::array<uint16_t, 5> pwm_amounts_{};
  const uint8_t max_power_color_channels_;
  const uint8_t max_power_white_channels_;
};

}  // namespace esphome::bp1658cj

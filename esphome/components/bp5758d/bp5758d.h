#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include <vector>
#include <array>

namespace esphome::bp5758d {

class BP5758D final : public Component {
 public:
  void set_data_pin(GPIOPin *data_pin) { data_pin_ = data_pin; }
  void set_clock_pin(GPIOPin *clock_pin) { clock_pin_ = clock_pin; }

  void setup() override;

  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  /// Send new values if they were updated.
  void loop() override;

  /// Set channel (0-based, OUT1 is 0) to state (0.0 to 1.0) at the given current.
  void write_channel(uint8_t channel, float state, uint8_t current);

 protected:
  uint8_t correct_current_level_bits_(uint8_t current);
  void write_bit_(bool value);
  void write_byte_(uint8_t data);
  void write_buffer_(uint8_t *buffer, uint8_t size);

  GPIOPin *data_pin_;
  GPIOPin *clock_pin_;
  std::array<uint8_t, 5> channel_current_{};
  std::array<uint16_t, 5> pwm_amounts_{};
  bool update_{true};
};

}  // namespace esphome::bp5758d

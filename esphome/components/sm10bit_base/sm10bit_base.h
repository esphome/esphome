#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/output/float_output.h"
#include <vector>

namespace esphome::sm10bit_base {

class Sm10BitBase : public Component {
 public:
  void set_model(uint8_t model_id) { model_id_ = model_id; }
  void set_data_pin(GPIOPin *data_pin) { data_pin_ = data_pin; }
  void set_clock_pin(GPIOPin *clock_pin) { clock_pin_ = clock_pin; }
  void set_max_power_color_channels(uint8_t max_power_color_channels) {
    max_power_color_channels_ = max_power_color_channels;
  }
  void set_max_power_white_channels(uint8_t max_power_white_channels) {
    max_power_white_channels_ = max_power_white_channels;
  }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void setup() override;
  void dump_config() override;
  void loop() override;

  void write_channel(uint8_t channel, float state);

 protected:
  void write_bit_(bool value);
  void write_byte_(uint8_t data);
  void write_buffer_(uint8_t *buffer, uint8_t size);

  GPIOPin *data_pin_;
  GPIOPin *clock_pin_;
  uint8_t model_id_;
  uint8_t max_power_color_channels_{2};
  uint8_t max_power_white_channels_{4};
  uint8_t update_channel_;
  std::vector<uint16_t> pwm_amounts_;
  bool update_{true};
};

}  // namespace esphome::sm10bit_base

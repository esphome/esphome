#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/output/float_output.h"
#include <vector>

namespace esphome {
namespace bp1658cj {

class BP1658CJ : public Component {
 public:
  class Channel;

  void set_data_pin(GPIOPin *data_pin) { data_pin_ = data_pin; }
  void set_clock_pin(GPIOPin *clock_pin) { clock_pin_ = clock_pin; }
  void set_max_power_color_channels(uint8_t max_power_color_channels) {
    max_power_color_channels_ = max_power_color_channels;
  }
  void set_max_power_white_channels(uint8_t max_power_white_channels) {
    max_power_white_channels_ = max_power_white_channels;
  }

  void setup() override;

  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  /// Send new values if they were updated.
  void loop() override;

  class Channel : public output::FloatOutput {
   public:
    void set_parent(BP1658CJ *parent) { parent_ = parent; }
    void set_channel(uint8_t channel) { channel_ = channel; }

   protected:
    void write_state(float state) override {
      auto amount = static_cast<uint16_t>(state * 0x3FF);
      this->parent_->set_channel_value_(this->channel_, amount);
    }

    BP1658CJ *parent_;
    uint8_t channel_;
  };

 protected:
  void set_channel_value_(uint8_t channel, uint16_t value);
  void write_bit_(bool value);
  void write_byte_(uint8_t data);
  void write_buffer_(uint8_t *buffer, uint8_t size);

  GPIOPin *data_pin_;
  GPIOPin *clock_pin_;
  uint8_t max_power_color_channels_{4};
  uint8_t max_power_white_channels_{6};
  uint8_t update_channel_;
  std::vector<uint16_t> pwm_amounts_;
  bool update_{true};
};

}  // namespace bp1658cj
}  // namespace esphome

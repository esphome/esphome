#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/output/float_output.h"
#include <vector>

namespace esphome {
namespace sm16716 {

class SM16716 : public Component {
 public:
  class Channel;

  void set_data_pin(GPIOPin *data_pin) { data_pin_ = data_pin; }
  void set_clock_pin(GPIOPin *clock_pin) { clock_pin_ = clock_pin; }
  void set_num_channels(uint8_t num_channels) { num_channels_ = num_channels; }
  void set_num_chips(uint8_t num_chips) { num_chips_ = num_chips; }

  void setup() override;

  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  /// Send new values if they were updated.
  void loop() override;

  class Channel : public output::FloatOutput {
   public:
    void set_parent(SM16716 *parent) { parent_ = parent; }
    void set_channel(uint8_t channel) { channel_ = channel; }

   protected:
    void write_state(float state) override {
      auto amount = uint8_t(state * 0xFF);
      this->parent_->set_channel_value_(this->channel_, amount);
    }

    SM16716 *parent_;
    uint8_t channel_;
  };

 protected:
  void set_channel_value_(uint8_t channel, uint8_t value) {
    uint8_t index = this->num_channels_ - channel - 1;
    if (this->pwm_amounts_[index] != value) {
      this->update_ = true;
    }
    this->pwm_amounts_[index] = value;
  }
  void write_bit_(bool value) {
    this->data_pin_->digital_write(value);
    this->clock_pin_->digital_write(true);
    this->clock_pin_->digital_write(false);
  }
  void write_byte_(uint8_t data) {
    for (uint8_t mask = 0x80; mask; mask >>= 1) {
      this->write_bit_(data & mask);
    }
  }

  GPIOPin *data_pin_;
  GPIOPin *clock_pin_;
  uint8_t num_channels_;
  uint8_t num_chips_;
  std::vector<uint8_t> pwm_amounts_;
  bool update_{true};
};

}  // namespace sm16716
}  // namespace esphome

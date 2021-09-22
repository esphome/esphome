#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/output/float_output.h"
#include <vector>

namespace esphome {
namespace sm2135 {

class SM2135 : public Component {
 public:
  class Channel;

  void set_data_pin(GPIOPin *data_pin) { data_pin_ = data_pin; }
  void set_clock_pin(GPIOPin *clock_pin) { clock_pin_ = clock_pin; }

  void setup() override;

  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  /// Send new values if they were updated.
  void loop() override;

  class Channel : public output::FloatOutput {
   public:
    void set_parent(SM2135 *parent) { parent_ = parent; }
    void set_channel(uint8_t channel) { channel_ = channel; }

   protected:
    void write_state(float state) override {
      auto amount = static_cast<uint8_t>(state * 0xff);
      this->parent_->set_channel_value_(this->channel_, amount);
    }

    SM2135 *parent_;
    uint8_t channel_;
  };

 protected:
  void set_channel_value_(uint8_t channel, uint8_t value) {
    if (this->pwm_amounts_[channel] != value) {
      this->update_ = true;
      this->update_channel_ = channel;
    }
    this->pwm_amounts_[channel] = value;
  }
  void write_bit_(bool value) {
    this->clock_pin_->digital_write(false);
    this->data_pin_->digital_write(value);
    this->clock_pin_->digital_write(true);
  }

  void write_byte_(uint8_t data) {
    for (uint8_t mask = 0x80; mask; mask >>= 1) {
      this->write_bit_(data & mask);
    }
    this->clock_pin_->digital_write(false);
    this->data_pin_->digital_write(true);
    this->clock_pin_->digital_write(true);
  }

  void write_buffer_(uint8_t *buffer, uint8_t size) {
    this->data_pin_->digital_write(false);
    for (uint32_t i = 0; i < size; i++) {
      this->write_byte_(buffer[i]);
    }
    this->clock_pin_->digital_write(false);
    this->clock_pin_->digital_write(true);
    this->data_pin_->digital_write(true);
  }

  GPIOPin *data_pin_;
  GPIOPin *clock_pin_;
  uint8_t update_channel_;
  std::vector<uint8_t> pwm_amounts_;
  bool update_{true};
};

}  // namespace sm2135
}  // namespace esphome

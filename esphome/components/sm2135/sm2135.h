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

  void set_rgb_current(uint8 rgb_current) {
   rgb_current_ = rgb_current;
   current_mask_ = (convert_ma_to_bitmask(rgb_current_) << 4) | \
     convert_ma_to_bitmask(cw_current_);
  }

  void set_cw_current(uint8 cw_current) {
   cw_current_ = cw_current;
   current_mask_ = (convert_ma_to_bitmask(rgb_current_) << 4) | \
     convert_ma_to_bitmask(cw_current_);
  }

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

  const uint8_t SM2135_DELAY = 4;

  void set_channel_value_(uint8_t channel, uint8_t value) {
    if (this->pwm_amounts_[channel] != value) {
      this->update_ = true;
      this->update_channel_ = channel;
    }
    this->pwm_amounts_[channel] = value;
  }

  void Sm2135SetLow_(GPIOPin *pin) {
    pin->digital_write(false);
    pin->pin_mode(gpio::FLAG_OUTPUT);
  }

  void Sm2135SetHigh_(GPIOPin *pin) {
      pin->pin_mode(gpio::FLAG_PULLUP);
  }

  void Sm2135Start_(void) {
    Sm2135SetLow_(this->data_pin_);
    delayMicroseconds(SM2135_DELAY);
    Sm2135SetLow_(this->clock_pin_);
  }

  void Sm2135Stop_(void) {
    Sm2135SetLow_(this->data_pin_);
    delayMicroseconds(SM2135_DELAY);
    Sm2135SetHigh_(this->clock_pin_);
    delayMicroseconds(SM2135_DELAY);
    Sm2135SetHigh_(this->data_pin_);
    delayMicroseconds(SM2135_DELAY);
  }

  void write_byte_(uint8_t data) {
    for (uint8_t mask = 0x80; mask; mask >>= 1) {
      if(mask & data) {
        Sm2135SetHigh_(this->data_pin_);
      } else {
        Sm2135SetLow_(this->data_pin_);
      }

      Sm2135SetHigh_(clock_pin_);
      delayMicroseconds(SM2135_DELAY);
      Sm2135SetLow_(clock_pin_);
    }

    Sm2135SetHigh_(this->data_pin_);
    Sm2135SetHigh_(this->clock_pin_);
    delayMicroseconds(SM2135_DELAY / 2);
    Sm2135SetLow_(this->clock_pin_);
    delayMicroseconds(SM2135_DELAY / 2);
    Sm2135SetLow_(this->data_pin_);
  }

  void write_buffer_(uint8_t *buffer, uint8_t size) {
    Sm2135Start_();

    this->data_pin_->digital_write(false);
    for (uint32_t i = 0; i < size; i++) {
      this->write_byte_(buffer[i]);
    }

    Sm2135Stop_();
  }

  uint8_t convert_ma_to_bitmask(uint8_t ma) {
    return (ma-10) / 5;
  }

  GPIOPin *data_pin_;
  GPIOPin *clock_pin_;
  uint8 current_mask_;
  uint8 rgb_current_;
  uint8 cw_current_;
  uint8_t update_channel_;
  std::vector<uint8_t> pwm_amounts_;
  bool update_{true};

};

}  // namespace sm2135
}  // namespace esphome

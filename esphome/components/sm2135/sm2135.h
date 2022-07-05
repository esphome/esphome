#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/output/float_output.h"
#include <vector>
#include <map>

namespace esphome {
namespace sm2135 {

static const char *const TAG = "sm2135";

static const uint8_t SM2135_ADDR_MC = 0xC0;  // Max current register
static const uint8_t SM2135_ADDR_CH = 0xC1;  // RGB or CW channel select register

// NOTE: This is the default chip byte order of colors, but SM2135::loop() uses a different order.
static const uint8_t SM2135_ADDR_R = 0xC2;   // Red color
static const uint8_t SM2135_ADDR_G = 0xC3;   // Green color
static const uint8_t SM2135_ADDR_B = 0xC4;   // Blue color
static const uint8_t SM2135_ADDR_C = 0xC5;   // Cold
static const uint8_t SM2135_ADDR_W = 0xC6;   // Warm

static const uint8_t SM2135_RGB = 0x00;  // RGB channel
static const uint8_t SM2135_CW = 0x80;   // CW channel (Chip default)

static const std::map<uint8_t,uint8_t> SM2135_CURRENT = {
  { 10, 0x00 },
  { 15, 0x01 },
  { 20, 0x02 },
  { 25, 0x03 },
  { 30, 0x04 },
  { 35, 0x05 },
  { 40, 0x06 },
  { 45, 0x07 },  // Max value for RGB
  { 50, 0x08 },
  { 55, 0x09 },
  { 60, 0x0A },
};

#define SM2135_CURRENT_LOOKUP(ma) ( SM2135_CURRENT.find(ma)->second )

class SM2135 : public Component {
 public:
  class Channel;

  void set_data_pin(GPIOPin *data_pin) { data_pin_ = data_pin; }
  void set_clock_pin(GPIOPin *clock_pin) { clock_pin_ = clock_pin; }

  void set_rgb_current(uint8_t rgb_current) {
    rgb_current_ = rgb_current;
    current_mask_ = (SM2135_CURRENT_LOOKUP(rgb_current_) << 4) | SM2135_CURRENT_LOOKUP(cw_current_);
  }

  void set_cw_current(uint8_t cw_current) {
    cw_current_ = cw_current;
    current_mask_ = (SM2135_CURRENT_LOOKUP(rgb_current_) << 4) | SM2135_CURRENT_LOOKUP(cw_current_);
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
  const uint8_t s_m2135_delay_ = 4;

  void set_channel_value_(uint8_t channel, uint8_t value) {
    if (this->pwm_amounts_[channel] != value) {
      this->update_ = true;
      this->update_channel_ = channel;
    }
    this->pwm_amounts_[channel] = value;
  }

  void sm2135_set_low_(GPIOPin *pin) {
    pin->digital_write(false);
    pin->pin_mode(gpio::FLAG_OUTPUT);
  }

  void sm2135_set_high_(GPIOPin *pin) {
    pin->digital_write(true);
    pin->pin_mode(gpio::FLAG_PULLUP);
  }

  void sm2135_start_() {
    sm2135_set_low_(this->data_pin_);
    delayMicroseconds(s_m2135_delay_);
    sm2135_set_low_(this->clock_pin_);
  }

  void sm2135_stop_() {
    sm2135_set_low_(this->data_pin_);
    delayMicroseconds(s_m2135_delay_);
    sm2135_set_high_(this->clock_pin_);
    delayMicroseconds(s_m2135_delay_);
    sm2135_set_high_(this->data_pin_);
    delayMicroseconds(s_m2135_delay_);
  }

  void write_byte_(uint8_t data) {
    for (uint8_t mask = 0x80; mask; mask >>= 1) {
      if (mask & data) {
        sm2135_set_high_(this->data_pin_);
      } else {
        sm2135_set_low_(this->data_pin_);
      }

      sm2135_set_high_(clock_pin_);
      delayMicroseconds(s_m2135_delay_);
      sm2135_set_low_(clock_pin_);
    }

    sm2135_set_high_(this->data_pin_);
    sm2135_set_high_(this->clock_pin_);
    delayMicroseconds(s_m2135_delay_ / 2);
    sm2135_set_low_(this->clock_pin_);
    delayMicroseconds(s_m2135_delay_ / 2);
    sm2135_set_low_(this->data_pin_);
  }

  void write_buffer_(uint8_t *buffer, uint8_t size) {
    sm2135_start_();

    this->data_pin_->digital_write(false);
    for (uint32_t i = 0; i < size; i++) {
      this->write_byte_(buffer[i]);
    }

    sm2135_stop_();
  }

  GPIOPin *data_pin_;
  GPIOPin *clock_pin_;
  uint8_t current_mask_;
  uint8_t rgb_current_;
  uint8_t cw_current_;
  uint8_t update_channel_;
  std::vector<uint8_t> pwm_amounts_;
  bool update_{true};
};

}  // namespace sm2135
}  // namespace esphome

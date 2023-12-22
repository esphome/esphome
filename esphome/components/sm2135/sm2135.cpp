#include "sm2135.h"
#include "esphome/core/log.h"

// Tnx to the work of https://github.com/arendst (Tasmota) for making the initial version of the driver

namespace esphome {
namespace sm2135 {

static const char *const TAG = "sm2135";

static const uint8_t SM2135_ADDR_MC = 0xC0;  // Max current register
static const uint8_t SM2135_ADDR_CH = 0xC1;  // RGB or CW channel select register
static const uint8_t SM2135_ADDR_R = 0xC2;   // Red color
static const uint8_t SM2135_ADDR_G = 0xC3;   // Green color
static const uint8_t SM2135_ADDR_B = 0xC4;   // Blue color
static const uint8_t SM2135_ADDR_C = 0xC5;   // Cold
static const uint8_t SM2135_ADDR_W = 0xC6;   // Warm

static const uint8_t SM2135_RGB = 0x00;  // RGB channel
static const uint8_t SM2135_CW = 0x80;   // CW channel (Chip default)

void SM2135::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SM2135OutputComponent...");
  this->data_pin_->setup();
  this->data_pin_->digital_write(false);
  this->data_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->clock_pin_->setup();
  this->clock_pin_->digital_write(false);
  this->data_pin_->pin_mode(gpio::FLAG_OUTPUT);

  this->data_pin_->pin_mode(gpio::FLAG_PULLUP);
  this->clock_pin_->pin_mode(gpio::FLAG_PULLUP);

  this->pwm_amounts_.resize(5, 0);
}

void SM2135::dump_config() {
  ESP_LOGCONFIG(TAG, "SM2135:");
  LOG_PIN("  Data Pin: ", this->data_pin_);
  LOG_PIN("  Clock Pin: ", this->clock_pin_);
  ESP_LOGCONFIG(TAG, "  CW Current: %dmA", 10 + (this->cw_current_ * 5));
  ESP_LOGCONFIG(TAG, "  RGB Current: %dmA", 10 + (this->rgb_current_ * 5));
}

void SM2135::write_byte_(uint8_t data) {
  for (uint8_t mask = 0x80; mask; mask >>= 1) {
    if (mask & data) {
      this->sm2135_set_high_(this->data_pin_);
    } else {
      this->sm2135_set_low_(this->data_pin_);
    }

    this->sm2135_set_high_(this->clock_pin_);
    delayMicroseconds(4);
    this->sm2135_set_low_(clock_pin_);
  }

  this->sm2135_set_high_(this->data_pin_);
  this->sm2135_set_high_(this->clock_pin_);
  delayMicroseconds(2);
  this->sm2135_set_low_(this->clock_pin_);
  delayMicroseconds(2);
  this->sm2135_set_low_(this->data_pin_);
}

void SM2135::sm2135_start_() {
  this->sm2135_set_low_(this->data_pin_);
  delayMicroseconds(4);
  this->sm2135_set_low_(this->clock_pin_);
}

void SM2135::sm2135_stop_() {
  this->sm2135_set_low_(this->data_pin_);
  delayMicroseconds(4);
  this->sm2135_set_high_(this->clock_pin_);
  delayMicroseconds(4);
  this->sm2135_set_high_(this->data_pin_);
  delayMicroseconds(4);
}

void SM2135::write_buffer_(uint8_t *buffer, uint8_t size) {
  this->sm2135_start_();

  this->data_pin_->digital_write(false);
  for (uint32_t i = 0; i < size; i++) {
    this->write_byte_(buffer[i]);
  }

  this->sm2135_stop_();
}

void SM2135::loop() {
  if (!this->update_)
    return;

  this->sm2135_start_();
  this->write_byte_(SM2135_ADDR_MC);
  this->write_byte_(current_mask_);

  if (this->update_channel_ == 3 || this->update_channel_ == 4) {
    // No color so must be Cold/Warm

    this->write_byte_(SM2135_CW);
    this->sm2135_stop_();
    delay(1);
    this->sm2135_start_();
    this->write_byte_(SM2135_ADDR_C);
    this->write_byte_(this->pwm_amounts_[4]);  // Warm
    this->write_byte_(this->pwm_amounts_[3]);  // Cold
  } else {
    // Color

    this->write_byte_(SM2135_RGB);
    this->write_byte_(this->pwm_amounts_[1]);  // Green
    this->write_byte_(this->pwm_amounts_[0]);  // Red
    this->write_byte_(this->pwm_amounts_[2]);  // Blue
  }

  this->sm2135_stop_();

  this->update_ = false;
}

void SM2135::set_channel_value_(uint8_t channel, uint8_t value) {
  if (this->pwm_amounts_[channel] != value) {
    this->update_ = true;
    this->update_channel_ = channel;
  }
  this->pwm_amounts_[channel] = value;
}

void SM2135::sm2135_set_low_(GPIOPin *pin) {
  pin->digital_write(false);
  pin->pin_mode(gpio::FLAG_OUTPUT);
}

void SM2135::sm2135_set_high_(GPIOPin *pin) {
  pin->digital_write(true);
  pin->pin_mode(gpio::FLAG_PULLUP);
}

}  // namespace sm2135
}  // namespace esphome

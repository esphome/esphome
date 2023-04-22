#include "sm2135.h"
#include "esphome/core/log.h"

// Tnx to the work of https://github.com/arendst (Tasmota) for making the initial version of the driver

namespace esphome {
namespace sm2135 {

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
  ESP_LOGCONFIG(TAG, "  CW Current: %d", this->cw_current_);
  ESP_LOGCONFIG(TAG, "  RGB Current: %d", this->rgb_current_);
}

void SM2135::write_byte_(uint8_t data) {
  for (uint8_t mask = 0x80; mask; mask >>= 1) {
    if (mask & data) {
      sm2135_set_high_(this->data_pin_);
    } else {
      sm2135_set_low_(this->data_pin_);
    }

    sm2135_set_high_(clock_pin_);
    delayMicroseconds(sm2135_delay_);
    sm2135_set_low_(clock_pin_);
  }

  sm2135_set_high_(this->data_pin_);
  sm2135_set_high_(this->clock_pin_);
  delayMicroseconds(sm2135_delay_ / 2);
  sm2135_set_low_(this->clock_pin_);
  delayMicroseconds(sm2135_delay_ / 2);
  sm2135_set_low_(this->data_pin_);
}

void SM2135::sm2135_start_() {
  sm2135_set_low_(this->data_pin_);
  delayMicroseconds(sm2135_delay_);
  sm2135_set_low_(this->clock_pin_);
}

void SM2135::sm2135_stop_() {
  sm2135_set_low_(this->data_pin_);
  delayMicroseconds(sm2135_delay_);
  sm2135_set_high_(this->clock_pin_);
  delayMicroseconds(sm2135_delay_);
  sm2135_set_high_(this->data_pin_);
  delayMicroseconds(sm2135_delay_);
}

void SM2135::write_buffer_(uint8_t *buffer, uint8_t size) {
  sm2135_start_();

  this->data_pin_->digital_write(false);
  for (uint32_t i = 0; i < size; i++) {
    this->write_byte_(buffer[i]);
  }

  sm2135_stop_();
}

void SM2135::loop() {
  if (!this->update_)
    return;

  sm2135_start_();
  write_byte_(SM2135_ADDR_MC);
  write_byte_(current_mask_);

  if (this->update_channel_ == 3 || this->update_channel_ == 4) {
    // No color so must be Cold/Warm

    write_byte_(SM2135_CW);
    sm2135_stop_();
    delay(1);
    sm2135_start_();
    write_byte_(SM2135_ADDR_C);
    write_byte_(this->pwm_amounts_[4]);  // Warm
    write_byte_(this->pwm_amounts_[3]);  // Cold
  } else {
    // Color

    write_byte_(SM2135_RGB);
    write_byte_(this->pwm_amounts_[1]);  // Green
    write_byte_(this->pwm_amounts_[0]);  // Red
    write_byte_(this->pwm_amounts_[2]);  // Blue
  }

  sm2135_stop_();

  this->update_ = false;
}

}  // namespace sm2135
}  // namespace esphome

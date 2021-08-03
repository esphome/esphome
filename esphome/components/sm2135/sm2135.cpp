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

static const uint8_t SM2135_10MA = 0x00;
static const uint8_t SM2135_15MA = 0x01;
static const uint8_t SM2135_20MA = 0x02;  // RGB max current (Chip default)
static const uint8_t SM2135_25MA = 0x03;
static const uint8_t SM2135_30MA = 0x04;  // CW max current (Chip default)
static const uint8_t SM2135_35MA = 0x05;
static const uint8_t SM2135_40MA = 0x06;
static const uint8_t SM2135_45MA = 0x07;  // Max value for RGB
static const uint8_t SM2135_50MA = 0x08;
static const uint8_t SM2135_55MA = 0x09;
static const uint8_t SM2135_60MA = 0x0A;

static const uint8_t SM2135_CURRENT = (SM2135_20MA << 4) | SM2135_10MA;

void SM2135::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SM2135OutputComponent...");
  this->data_pin_->setup();
  this->data_pin_->digital_write(true);
  this->clock_pin_->setup();
  this->clock_pin_->digital_write(true);
  this->pwm_amounts_.resize(5, 0);
}
void SM2135::dump_config() {
  ESP_LOGCONFIG(TAG, "SM2135:");
  LOG_PIN("  Data Pin: ", this->data_pin_);
  LOG_PIN("  Clock Pin: ", this->clock_pin_);
}

void SM2135::loop() {
  if (!this->update_)
    return;

  uint8_t data[6];
  if (this->update_channel_ == 3 || this->update_channel_ == 4) {
    // No color so must be Cold/Warm
    data[0] = SM2135_ADDR_MC;
    data[1] = SM2135_CURRENT;
    data[2] = SM2135_CW;
    this->write_buffer_(data, 3);
    delay(1);
    data[0] = SM2135_ADDR_C;
    data[1] = this->pwm_amounts_[4];  // Warm
    data[2] = this->pwm_amounts_[3];  // Cold
    this->write_buffer_(data, 3);
  } else {
    // Color
    data[0] = SM2135_ADDR_MC;
    data[1] = SM2135_CURRENT;
    data[2] = SM2135_RGB;
    data[3] = this->pwm_amounts_[1];  // Green
    data[4] = this->pwm_amounts_[0];  // Red
    data[5] = this->pwm_amounts_[2];  // Blue
    this->write_buffer_(data, 6);
  }

  this->update_ = false;
}

}  // namespace sm2135
}  // namespace esphome

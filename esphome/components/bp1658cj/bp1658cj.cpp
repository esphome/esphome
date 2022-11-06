#include "bp1658cj.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bp1658cj {

static const char *const TAG = "bp1658cj";

static const uint8_t BP1658CJ_MODEL_ID = 0x80;
static const uint8_t BP1658CJ_ADDR_STANDBY = 0x0;
static const uint8_t BP1658CJ_ADDR_START_3CH = 0x10;
static const uint8_t BP1658CJ_ADDR_START_2CH = 0x20;
static const uint8_t BP1658CJ_ADDR_START_5CH = 0x30;

void BP1658CJ::setup() {
  ESP_LOGCONFIG(TAG, "Setting up BP1658CJ Output Component...");
  this->data_pin_->setup();
  this->data_pin_->digital_write(false);
  this->clock_pin_->setup();
  this->clock_pin_->digital_write(false);
  this->pwm_amounts_.resize(5, 0);
}
void BP1658CJ::dump_config() {
  ESP_LOGCONFIG(TAG, "BP1658CJ:");
  LOG_PIN("  Data Pin: ", this->data_pin_);
  LOG_PIN("  Clock Pin: ", this->clock_pin_);
  ESP_LOGCONFIG(TAG, "  Color Channels Max Power: %u", this->max_power_color_channels_);
  ESP_LOGCONFIG(TAG, "  White Channels Max Power: %u", this->max_power_white_channels_);
}

void BP1658CJ::loop() {
  if (!this->update_)
    return;

  uint8_t data[12];
  if (this->pwm_amounts_[0] == 0 && this->pwm_amounts_[1] == 0 && this->pwm_amounts_[2] == 0 &&
      this->pwm_amounts_[3] == 0 && this->pwm_amounts_[4] == 0) {
    // Off / Sleep
    data[0] = BP1658CJ_MODEL_ID + BP1658CJ_ADDR_STANDBY;
    for (int i = 1; i < 12; i++)
      data[i] = 0;
    this->write_buffer_(data, 12);
  } else if (this->pwm_amounts_[0] == 0 && this->pwm_amounts_[1] == 0 && this->pwm_amounts_[2] == 0 &&
             (this->pwm_amounts_[3] > 0 || this->pwm_amounts_[4] > 0)) {
    // Only data on white channels
    data[0] = BP1658CJ_MODEL_ID + BP1658CJ_ADDR_START_2CH;
    data[1] = 0 << 4 | this->max_power_white_channels_;
    for (int i = 2, j = 0; i < 12; i += 2, j++) {
      data[i] = this->pwm_amounts_[j] & 0x1F;
      data[i + 1] = (this->pwm_amounts_[j] >> 5) & 0x1F;
    }
    this->write_buffer_(data, 12);
  } else if ((this->pwm_amounts_[0] > 0 || this->pwm_amounts_[1] > 0 || this->pwm_amounts_[2] > 0) &&
             this->pwm_amounts_[3] == 0 && this->pwm_amounts_[4] == 0) {
    // Only data on RGB channels
    data[0] = BP1658CJ_MODEL_ID + BP1658CJ_ADDR_START_3CH;
    data[1] = this->max_power_color_channels_ << 4 | 0;
    for (int i = 2, j = 0; i < 12; i += 2, j++) {
      data[i] = this->pwm_amounts_[j] & 0x1F;
      data[i + 1] = (this->pwm_amounts_[j] >> 5) & 0x1F;
    }
    this->write_buffer_(data, 12);
  } else {
    // All channels
    data[0] = BP1658CJ_MODEL_ID + BP1658CJ_ADDR_START_5CH;
    data[1] = this->max_power_color_channels_ << 4 | this->max_power_white_channels_;
    for (int i = 2, j = 0; i < 12; i += 2, j++) {
      data[i] = this->pwm_amounts_[j] & 0x1F;
      data[i + 1] = (this->pwm_amounts_[j] >> 5) & 0x1F;
    }
    this->write_buffer_(data, 12);
  }

  this->update_ = false;
}

void BP1658CJ::set_channel_value_(uint8_t channel, uint16_t value) {
  if (this->pwm_amounts_[channel] != value) {
    this->update_ = true;
    this->update_channel_ = channel;
  }
  this->pwm_amounts_[channel] = value;
}
void BP1658CJ::write_bit_(bool value) {
  this->clock_pin_->digital_write(false);
  this->data_pin_->digital_write(value);
  this->clock_pin_->digital_write(true);
}

void BP1658CJ::write_byte_(uint8_t data) {
  for (uint8_t mask = 0x80; mask; mask >>= 1) {
    this->write_bit_(data & mask);
  }
  this->clock_pin_->digital_write(false);
  this->data_pin_->digital_write(true);
  this->clock_pin_->digital_write(true);
}

void BP1658CJ::write_buffer_(uint8_t *buffer, uint8_t size) {
  this->data_pin_->digital_write(false);
  for (uint32_t i = 0; i < size; i++) {
    this->write_byte_(buffer[i]);
  }
  this->clock_pin_->digital_write(false);
  this->clock_pin_->digital_write(true);
  this->data_pin_->digital_write(true);
}

}  // namespace bp1658cj
}  // namespace esphome

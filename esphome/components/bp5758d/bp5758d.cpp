#include "bp5758d.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bp5758d {

static const char *const TAG = "bp5758d";

static const uint8_t BP5758D_MODEL_ID = 0b10000000;
static const uint8_t BP5758D_ADDR_STANDBY = 0b00000000;
// Note, channel start address seems ambiguous and mis-translated.
// Documentation states all are "invalid sleep"
// All 3 values appear to activate all 5 channels.  Using the mapping
// from BP1658CJ ordering since it won't break anything.
static const uint8_t BP5758D_ADDR_START_3CH = 0b00010000;
static const uint8_t BP5758D_ADDR_START_2CH = 0b00100000;
static const uint8_t BP5758D_ADDR_START_5CH = 0b00110000;
static const uint8_t BP5758D_ALL_DATA_CHANNEL_ENABLEMENT = 0b00011111;

void BP5758D::setup() {
  ESP_LOGCONFIG(TAG, "Setting up BP5758D Output Component...");
  this->data_pin_->setup();
  this->data_pin_->digital_write(false);
  this->clock_pin_->setup();
  this->clock_pin_->digital_write(false);
  this->channel_current_.resize(5, 0);
  this->pwm_amounts_.resize(5, 0);
}
void BP5758D::dump_config() {
  ESP_LOGCONFIG(TAG, "BP5758D:");
  LOG_PIN("  Data Pin: ", this->data_pin_);
  LOG_PIN("  Clock Pin: ", this->clock_pin_);
}

void BP5758D::loop() {
  if (!this->update_)
    return;

  uint8_t data[17];
  if (this->pwm_amounts_[0] == 0 && this->pwm_amounts_[1] == 0 && this->pwm_amounts_[2] == 0 &&
      this->pwm_amounts_[3] == 0 && this->pwm_amounts_[4] == 0) {
    // Off / Sleep
    data[0] = BP5758D_MODEL_ID + BP5758D_ADDR_STANDBY;
    for (int i = 1; i < 16; i++)
      data[i] = 0;
    this->write_buffer_(data, 17);
  } else if (this->pwm_amounts_[0] == 0 && this->pwm_amounts_[1] == 0 && this->pwm_amounts_[2] == 0 &&
             (this->pwm_amounts_[3] > 0 || this->pwm_amounts_[4] > 0)) {
    // Only data on white channels
    data[0] = BP5758D_MODEL_ID + BP5758D_ADDR_START_2CH;
    data[1] = BP5758D_ALL_DATA_CHANNEL_ENABLEMENT;
    data[2] = 0;
    data[3] = 0;
    data[4] = 0;
    data[5] = this->pwm_amounts_[3] > 0 ? correct_current_level_bits_(this->channel_current_[3]) : 0;
    data[6] = this->pwm_amounts_[4] > 0 ? correct_current_level_bits_(this->channel_current_[4]) : 0;
    for (int i = 7, j = 0; i <= 15; i += 2, j++) {
      data[i] = this->pwm_amounts_[j] & 0x1F;
      data[i + 1] = (this->pwm_amounts_[j] >> 5) & 0x1F;
    }
    this->write_buffer_(data, 17);
  } else if ((this->pwm_amounts_[0] > 0 || this->pwm_amounts_[1] > 0 || this->pwm_amounts_[2] > 0) &&
             this->pwm_amounts_[3] == 0 && this->pwm_amounts_[4] == 0) {
    // Only data on RGB channels
    data[0] = BP5758D_MODEL_ID + BP5758D_ADDR_START_3CH;
    data[1] = BP5758D_ALL_DATA_CHANNEL_ENABLEMENT;
    data[2] = this->pwm_amounts_[0] > 0 ? correct_current_level_bits_(this->channel_current_[0]) : 0;
    data[3] = this->pwm_amounts_[1] > 0 ? correct_current_level_bits_(this->channel_current_[1]) : 0;
    data[4] = this->pwm_amounts_[2] > 0 ? correct_current_level_bits_(this->channel_current_[2]) : 0;
    data[5] = 0;
    data[6] = 0;
    for (int i = 7, j = 0; i <= 15; i += 2, j++) {
      data[i] = this->pwm_amounts_[j] & 0x1F;
      data[i + 1] = (this->pwm_amounts_[j] >> 5) & 0x1F;
    }
    this->write_buffer_(data, 17);
  } else {
    // All channels
    data[0] = BP5758D_MODEL_ID + BP5758D_ADDR_START_5CH;
    data[1] = BP5758D_ALL_DATA_CHANNEL_ENABLEMENT;
    data[2] = this->pwm_amounts_[0] > 0 ? correct_current_level_bits_(this->channel_current_[0]) : 0;
    data[3] = this->pwm_amounts_[1] > 0 ? correct_current_level_bits_(this->channel_current_[1]) : 0;
    data[4] = this->pwm_amounts_[2] > 0 ? correct_current_level_bits_(this->channel_current_[2]) : 0;
    data[5] = this->pwm_amounts_[3] > 0 ? correct_current_level_bits_(this->channel_current_[3]) : 0;
    data[6] = this->pwm_amounts_[4] > 0 ? correct_current_level_bits_(this->channel_current_[4]) : 0;
    for (int i = 7, j = 0; i <= 15; i += 2, j++) {
      data[i] = this->pwm_amounts_[j] & 0x1F;
      data[i + 1] = (this->pwm_amounts_[j] >> 5) & 0x1F;
    }
    this->write_buffer_(data, 17);
  }

  this->update_ = false;
}

uint8_t BP5758D::correct_current_level_bits_(uint8_t current) {
  // Anything below 64 uses normal bitmapping.
  if (current < 64) {
    return current;
  }

  // Anything above 63 needs to be offset by +34 because the driver remaps bit 7 (normally 64) to 30.
  // (no idea why(!) but it is documented)
  // Example:
  // integer 64 would normally put out 0b01000000 but here 0b01000000 = 30 whereas everything lower
  // is normal, so we add 34 to the integer where
  // integer 98 = 0b01100010 which is 30 (7th bit adjusted) + 34 (1st-6th bits).
  return current + 34;
}

void BP5758D::set_channel_value_(uint8_t channel, uint16_t value) {
  if (this->pwm_amounts_[channel] != value) {
    this->update_ = true;
    this->update_channel_ = channel;
  }
  this->pwm_amounts_[channel] = value;
}

void BP5758D::set_channel_current_(uint8_t channel, uint8_t current) { this->channel_current_[channel] = current; }

void BP5758D::write_bit_(bool value) {
  this->clock_pin_->digital_write(false);
  this->data_pin_->digital_write(value);
  this->clock_pin_->digital_write(true);
}

void BP5758D::write_byte_(uint8_t data) {
  for (uint8_t mask = 0x80; mask; mask >>= 1) {
    this->write_bit_(data & mask);
  }
  this->clock_pin_->digital_write(false);
  this->data_pin_->digital_write(true);
  this->clock_pin_->digital_write(true);
}

void BP5758D::write_buffer_(uint8_t *buffer, uint8_t size) {
  this->data_pin_->digital_write(false);
  for (uint32_t i = 0; i < size; i++) {
    this->write_byte_(buffer[i]);
  }
  this->clock_pin_->digital_write(false);
  this->clock_pin_->digital_write(true);
  this->data_pin_->digital_write(true);
}

}  // namespace bp5758d
}  // namespace esphome

#include "pca9685_output.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace pca9685 {

static const char *const TAG = "pca9685";

const uint8_t PCA9685_MODE_INVERTED = 0x10;
const uint8_t PCA9685_MODE_OUTPUT_ONACK = 0x08;
const uint8_t PCA9685_MODE_OUTPUT_TOTEM_POLE = 0x04;
const uint8_t PCA9685_MODE_OUTNE_HIGHZ = 0x02;
const uint8_t PCA9685_MODE_OUTNE_LOW = 0x01;

static const uint8_t PCA9685_REGISTER_SOFTWARE_RESET = 0x06;
static const uint8_t PCA9685_REGISTER_MODE1 = 0x00;
static const uint8_t PCA9685_REGISTER_MODE2 = 0x01;
static const uint8_t PCA9685_REGISTER_LED0 = 0x06;
static const uint8_t PCA9685_REGISTER_PRE_SCALE = 0xFE;

static const uint8_t PCA9685_MODE1_RESTART = 0b10000000;
static const uint8_t PCA9685_MODE1_AUTOINC = 0b00100000;
static const uint8_t PCA9685_MODE1_SLEEP = 0b00010000;

void PCA9685Output::setup() {
  ESP_LOGCONFIG(TAG, "Setting up PCA9685OutputComponent...");

  ESP_LOGV(TAG, "  Resetting devices...");
  if (!this->write_bytes(PCA9685_REGISTER_SOFTWARE_RESET, nullptr, 0)) {
    this->mark_failed();
    return;
  }

  if (!this->write_byte(PCA9685_REGISTER_MODE1, PCA9685_MODE1_RESTART | PCA9685_MODE1_AUTOINC)) {
    this->mark_failed();
    return;
  }
  if (!this->write_byte(PCA9685_REGISTER_MODE2, this->mode_)) {
    this->mark_failed();
    return;
  }

  int pre_scaler = static_cast<int>((25000000 / (4096 * this->frequency_)) - 1);
  if (pre_scaler > 255)
    pre_scaler = 255;
  if (pre_scaler < 3)
    pre_scaler = 3;

  ESP_LOGV(TAG, "  -> Prescaler: %d", pre_scaler);

  uint8_t mode1;
  if (!this->read_byte(PCA9685_REGISTER_MODE1, &mode1)) {
    this->mark_failed();
    return;
  }
  mode1 = (mode1 & ~PCA9685_MODE1_RESTART) | PCA9685_MODE1_SLEEP;
  if (!this->write_byte(PCA9685_REGISTER_MODE1, mode1)) {
    this->mark_failed();
    return;
  }
  if (!this->write_byte(PCA9685_REGISTER_PRE_SCALE, pre_scaler)) {
    this->mark_failed();
    return;
  }

  mode1 = (mode1 & ~PCA9685_MODE1_SLEEP) | PCA9685_MODE1_RESTART;
  if (!this->write_byte(PCA9685_REGISTER_MODE1, mode1)) {
    this->mark_failed();
    return;
  }
  delayMicroseconds(500);

  this->loop();
}

void PCA9685Output::dump_config() {
  ESP_LOGCONFIG(TAG, "PCA9685:");
  ESP_LOGCONFIG(TAG, "  Mode: 0x%02X", this->mode_);
  ESP_LOGCONFIG(TAG, "  Frequency: %.0f Hz", this->frequency_);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Setting up PCA9685 failed!");
  }
}

void PCA9685Output::loop() {
  if (this->min_channel_ == 0xFF || !this->update_)
    return;

  const uint16_t num_channels = this->max_channel_ - this->min_channel_ + 1;
  for (uint8_t channel = this->min_channel_; channel <= this->max_channel_; channel++) {
    uint16_t phase_begin = uint16_t(channel - this->min_channel_) / num_channels * 4096;
    uint16_t phase_end;
    uint16_t amount = this->pwm_amounts_[channel];
    if (amount == 0) {
      phase_end = 4096;
    } else if (amount >= 4096) {
      phase_begin = 4096;
      phase_end = 0;
    } else {
      phase_end = phase_begin + amount;
      if (phase_end >= 4096)
        phase_end -= 4096;
    }

    ESP_LOGVV(TAG, "Channel %02u: amount=%04u phase_begin=%04u phase_end=%04u", channel, amount, phase_begin,
              phase_end);

    uint8_t data[4];
    data[0] = phase_begin & 0xFF;
    data[1] = (phase_begin >> 8) & 0xFF;
    data[2] = phase_end & 0xFF;
    data[3] = (phase_end >> 8) & 0xFF;

    uint8_t reg = PCA9685_REGISTER_LED0 + 4 * channel;
    if (!this->write_bytes(reg, data, 4)) {
      this->status_set_warning();
      return;
    }
  }

  this->status_clear_warning();
  this->update_ = false;
}

void PCA9685Output::register_channel(PCA9685Channel *channel) {
  auto c = channel->channel_;
  this->min_channel_ = std::min(this->min_channel_, c);
  this->max_channel_ = std::max(this->max_channel_, c);
  channel->set_parent(this);
}

void PCA9685Channel::write_state(float state) {
  const uint16_t max_duty = 4096;
  const float duty_rounded = roundf(state * max_duty);
  auto duty = static_cast<uint16_t>(duty_rounded);
  this->parent_->set_channel_value_(this->channel_, duty);
}

}  // namespace pca9685
}  // namespace esphome

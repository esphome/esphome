#include "my9231.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace my9231 {

static const char *const TAG = "my9231.output";

// One-shot select (frame cycle repeat mode / frame cycle One-shot mode)
static const uint8_t MY9231_CMD_ONE_SHOT_DISABLE = 0x0 << 6;
static const uint8_t MY9231_CMD_ONE_SHOT_ENFORCE = 0x1 << 6;
// Reaction time of Iout
static const uint8_t MY9231_CMD_REACTION_FAST = 0x0 << 5;
static const uint8_t MY9231_CMD_REACTION_SLOW = 0x1 << 5;
// Grayscale resolution select
static const uint8_t MY9231_CMD_BIT_WIDTH_16 = 0x0 << 3;
static const uint8_t MY9231_CMD_BIT_WIDTH_14 = 0x1 << 3;
static const uint8_t MY9231_CMD_BIT_WIDTH_12 = 0x2 << 3;
static const uint8_t MY9231_CMD_BIT_WIDTH_8 = 0x3 << 3;
// Internal oscillator freq. select (divider)
static const uint8_t MY9231_CMD_FREQUENCY_DIVIDE_1 = 0x0 << 1;
static const uint8_t MY9231_CMD_FREQUENCY_DIVIDE_4 = 0x1 << 1;
static const uint8_t MY9231_CMD_FREQUENCY_DIVIDE_16 = 0x2 << 1;
static const uint8_t MY9231_CMD_FREQUENCY_DIVIDE_64 = 0x3 << 1;
// Output waveform
static const uint8_t MY9231_CMD_SCATTER_APDM = 0x0 << 0;
static const uint8_t MY9231_CMD_SCATTER_PWM = 0x1 << 0;

void MY9231OutputComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MY9231OutputComponent...");
  this->pin_di_->setup();
  this->pin_di_->digital_write(false);
  this->pin_dcki_->setup();
  this->pin_dcki_->digital_write(false);
  this->pwm_amounts_.resize(this->num_channels_, 0);
  uint8_t command = 0;
  if (this->bit_depth_ <= 8) {
    this->bit_depth_ = 8;
    command |= MY9231_CMD_BIT_WIDTH_8;
  } else if (this->bit_depth_ <= 12) {
    this->bit_depth_ = 12;
    command |= MY9231_CMD_BIT_WIDTH_12;
  } else if (this->bit_depth_ <= 14) {
    this->bit_depth_ = 14;
    command |= MY9231_CMD_BIT_WIDTH_14;
  } else {
    this->bit_depth_ = 16;
    command |= MY9231_CMD_BIT_WIDTH_16;
  }
  command |=
      MY9231_CMD_SCATTER_APDM | MY9231_CMD_FREQUENCY_DIVIDE_1 | MY9231_CMD_REACTION_FAST | MY9231_CMD_ONE_SHOT_DISABLE;
  ESP_LOGV(TAG, "  Command: 0x%02X", command);

  {
    InterruptLock lock;
    this->send_dcki_pulses_(32 * this->num_chips_);
    this->init_chips_(command);
  }
  ESP_LOGV(TAG, "  Chips initialized.");
}
void MY9231OutputComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MY9231:");
  LOG_PIN("  DI Pin: ", this->pin_di_);
  LOG_PIN("  DCKI Pin: ", this->pin_dcki_);
  ESP_LOGCONFIG(TAG, "  Total number of channels: %u", this->num_channels_);
  ESP_LOGCONFIG(TAG, "  Number of chips: %u", this->num_chips_);
  ESP_LOGCONFIG(TAG, "  Bit depth: %u", this->bit_depth_);
}
void MY9231OutputComponent::loop() {
  if (!this->update_)
    return;

  {
    InterruptLock lock;
    for (auto pwm_amount : this->pwm_amounts_) {
      this->write_word_(pwm_amount, this->bit_depth_);
    }
    // Send 8 DI pulses. After 8 falling edges, the duty data are store.
    this->send_di_pulses_(8);
  }
  this->update_ = false;
}
void MY9231OutputComponent::set_channel_value_(uint8_t channel, uint16_t value) {
  ESP_LOGV(TAG, "set channels %u to %u", channel, value);
  uint8_t index = this->num_channels_ - channel - 1;
  if (this->pwm_amounts_[index] != value) {
    this->update_ = true;
  }
  this->pwm_amounts_[index] = value;
}
void MY9231OutputComponent::init_chips_(uint8_t command) {
  // Send 12 DI pulse. After 6 falling edges, the duty data are stored
  // and after 12 rising edges the command mode is activated.
  this->send_di_pulses_(12);
  delayMicroseconds(12);
  for (uint8_t i = 0; i < this->num_chips_; i++) {
    this->write_word_(command, 8);
  }
  // Send 16 DI pulse. After 14 falling edges, the command data are
  // stored and after 16 falling edges the duty mode is activated.
  this->send_di_pulses_(16);
  delayMicroseconds(12);
}
void MY9231OutputComponent::write_word_(uint16_t value, uint8_t bits) {
  for (uint8_t i = bits; i > 0; i--) {
    this->pin_di_->digital_write(value & (1 << (i - 1)));
    this->pin_dcki_->digital_write(!this->pin_dcki_->digital_read());
  }
}
void MY9231OutputComponent::send_di_pulses_(uint8_t count) {
  delayMicroseconds(12);
  for (uint8_t i = 0; i < count; i++) {
    this->pin_di_->digital_write(true);
    this->pin_di_->digital_write(false);
  }
}
void MY9231OutputComponent::send_dcki_pulses_(uint8_t count) {
  delayMicroseconds(12);
  for (uint8_t i = 0; i < count; i++) {
    this->pin_dcki_->digital_write(true);
    this->pin_dcki_->digital_write(false);
  }
}

}  // namespace my9231
}  // namespace esphome

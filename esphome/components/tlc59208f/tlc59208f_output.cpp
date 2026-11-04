#include "tlc59208f_output.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::tlc59208f {

ESPHOME_LOG_TAG(TAG, "tlc59208f");

// * marks register defaults
// 0*: Register auto increment disabled, 1: Register auto increment enabled
const uint8_t TLC59208F_MODE1_AI2 = (1 << 7);
// 0*: don't auto increment bit 1, 1: auto increment bit 1
const uint8_t TLC59208F_MODE1_AI1 = (1 << 6);
// 0*: don't auto increment bit 0, 1: auto increment bit 0
const uint8_t TLC59208F_MODE1_AI0 = (1 << 5);
// 0: normal mode, 1*: low power mode, osc off
const uint8_t TLC59208F_MODE1_SLEEP = (1 << 4);
// 0*: device doesn't respond to i2c bus sub-address 1, 1: responds
const uint8_t TLC59208F_MODE1_SUB1 = (1 << 3);
// 0*: device doesn't respond to i2c bus sub-address 2, 1: responds
const uint8_t TLC59208F_MODE1_SUB2 = (1 << 2);
// 0*: device doesn't respond to i2c bus sub-address 3, 1: responds
const uint8_t TLC59208F_MODE1_SUB3 = (1 << 1);
// 0: device doesn't respond to i2c all-call 3, 1*: responds to all-call
const uint8_t TLC59208F_MODE1_ALLCALL = (1 << 0);

// TLC59208F MODE2 constants are now inline constexpr in tlc59208f_output.h

// --- Special function ---
// Call address to perform software reset, no devices will ACK
const uint8_t TLC59208F_SWRST_ADDR = 0x96;  //(0x4b 7-bit addr + ~W)
const uint8_t TLC59208F_SWRST_SEQ[2] = {0xa5, 0x5a};

// --- Registers ---2
// Mode register 1
const uint8_t TLC59208F_REG_MODE1 = 0x00;
// Mode register 2
const uint8_t TLC59208F_REG_MODE2 = 0x01;
// PWM0
const uint8_t TLC59208F_REG_PWM0 = 0x02;
// Group PWM
const uint8_t TLC59208F_REG_GROUPPWM = 0x0a;
// Group Freq
const uint8_t TLC59208F_REG_GROUPFREQ = 0x0b;
// LEDOUTx registers
const uint8_t TLC59208F_REG_LEDOUT0 = 0x0c;
const uint8_t TLC59208F_REG_LEDOUT1 = 0x0d;
// Sub-address registers
const uint8_t TLC59208F_REG_SUBADR1 = 0x0e;  // default: 0x92 (8-bit addr)
const uint8_t TLC59208F_REG_SUBADR2 = 0x0f;  // default: 0x94 (8-bit addr)
const uint8_t TLC59208F_REG_SUBADR3 = 0x10;  // default: 0x98 (8-bit addr)
// All call address register
const uint8_t TLC59208F_REG_ALLCALLADR = 0x11;  // default: 0xd0 (8-bit addr)

// --- Output modes ---
static const uint8_t LDR_OFF = 0x00;
static const uint8_t LDR_ON = 0x01;
static const uint8_t LDR_PWM = 0x02;
static const uint8_t LDR_GRPPWM = 0x03;

void TLC59208FOutput::setup() {
  ESP_LOGV(TAG, "  Resetting all devices on the bus");

  // Reset all devices on the bus
  if (this->bus_->write_readv(TLC59208F_SWRST_ADDR >> 1, TLC59208F_SWRST_SEQ, sizeof TLC59208F_SWRST_SEQ, nullptr, 0) !=
      i2c::ERROR_OK) {
    this->mark_failed(LOG_STR("RESET failed"));
    return;
  }

  // Auto increment registers, and respond to all-call address
  if (!this->write_byte(TLC59208F_REG_MODE1, TLC59208F_MODE1_AI2 | TLC59208F_MODE1_ALLCALL)) {
    this->mark_failed(LOG_STR("MODE1 failed"));
    return;
  }
  if (!this->write_byte(TLC59208F_REG_MODE2, this->mode_)) {
    this->mark_failed(LOG_STR("MODE2 failed"));
    return;
  }
  // Set all 3 outputs to be individually controlled
  // TODO: think of a way to support group dimming
  if (!this->write_byte(TLC59208F_REG_LEDOUT0, (LDR_PWM << 6) | (LDR_PWM << 4) | (LDR_PWM << 2) | (LDR_PWM << 0))) {
    this->mark_failed(LOG_STR("LEDOUT0 failed"));
    return;
  }
  if (!this->write_byte(TLC59208F_REG_LEDOUT1, (LDR_PWM << 6) | (LDR_PWM << 4) | (LDR_PWM << 2) | (LDR_PWM << 0))) {
    this->mark_failed(LOG_STR("LEDOUT1 failed"));
    return;
  }
  delayMicroseconds(500);

  this->loop();
}

void TLC59208FOutput::dump_config() {
  ESP_LOGCONFIG(TAG,
                "TLC59208F:\n"
                "  Mode: 0x%02X",
                this->mode_);
}

void TLC59208FOutput::loop() {
  if (this->min_channel_ == 0xFF || !this->update_)
    return;

  for (uint8_t channel = this->min_channel_; channel <= this->max_channel_; channel++) {
    uint8_t pwm = this->pwm_amounts_[channel];
    ESP_LOGVV(TAG, "Channel %02u: pwm=%04u ", channel, pwm);

    uint8_t reg = TLC59208F_REG_PWM0 + channel;
    if (!this->write_byte(reg, pwm)) {
      this->status_set_warning();
      return;
    }
  }

  this->status_clear_warning();
  this->update_ = false;
}

void TLC59208FOutput::register_channel(uint8_t channel) {
  this->min_channel_ = std::min(this->min_channel_, channel);
  this->max_channel_ = std::max(this->max_channel_, channel);
}

void TLC59208FOutput::write_channel(uint8_t channel, float state) {
  auto duty = static_cast<uint8_t>(roundf(state * 255));
  if (this->pwm_amounts_[channel] != duty)
    this->update_ = true;
  this->pwm_amounts_[channel] = duty;
}

}  // namespace esphome::tlc59208f

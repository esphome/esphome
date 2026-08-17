#include "lp5562_output.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::lp5562 {

static const char *const TAG = "lp5562";

// see table 26 in the datasheet https://web.archive.org/web/20240327023921/https://www.ti.com/lit/ds/symlink/lp5562.pdf
static const uint8_t LP5562_REG_ENABLE = 0x00;
static const uint8_t LP5562_REG_CONFIG = 0x08;
static const uint8_t LP5562_REG_LED_MAP = 0x70;

// PWM duty-cycle registers, indexed the same way as the LP5562_CHANNELS enum in output.py
static const uint8_t LP5562_REG_PWM[4] = {
    0x02,  // blue
    0x03,  // green
    0x04,  // red
    0x0e,  // white
};

void LP5562Output::setup() {
  // power up the chip and enable the internal 32 kHz oscillator, see
  // https://github.com/m5stack/M5GFX/blob/b2422cdb4735159209c2097d425c0756505b9a81/src/M5GFX.cpp#L566
  if (!this->write_byte(LP5562_REG_ENABLE, 0x40)) {
    this->mark_failed();
    return;
  }
  delay(1);
  // use the internal clock, and drive every channel directly from its PWM register instead of the
  // internal blink/fade engines
  if (!this->write_byte(LP5562_REG_CONFIG, 0x01) || !this->write_byte(LP5562_REG_LED_MAP, 0x00)) {
    this->mark_failed();
  }
}

void LP5562Output::dump_config() {
  ESP_LOGCONFIG(TAG, "LP5562:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Setting up LP5562 failed!");
  }
}

void LP5562Output::set_channel_value_(uint8_t channel, uint8_t value) {
  if (!this->write_byte(LP5562_REG_PWM[channel], value)) {
    this->status_set_warning();
    return;
  }
  this->status_clear_warning();
}

void LP5562Output::register_channel(LP5562Channel *channel) { channel->set_parent(this); }

void LP5562Channel::write_state(float state) {
  const uint8_t max_duty = 255;
  const auto duty = static_cast<uint8_t>(roundf(state * max_duty));
  this->parent_->set_channel_value_(this->channel_, duty);
}

}  // namespace esphome::lp5562

#include "lp5562_output.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::lp5562 {

ESPHOME_LOG_TAG(TAG, "lp5562");

// see table 26 in the datasheet https://web.archive.org/web/20240327023921/https://www.ti.com/lit/ds/symlink/lp5562.pdf
static constexpr uint8_t LP5562_REG_ENABLE = 0x00;
static constexpr uint8_t LP5562_REG_CONFIG = 0x08;
static constexpr uint8_t LP5562_REG_RESET = 0x0d;
static constexpr uint8_t LP5562_REG_LED_MAP = 0x70;

static constexpr uint8_t LP5562_RESET_VALUE = 0xff;

static constexpr uint8_t LP5562_ENABLE_CHIP_EN = 0x40;
static constexpr uint8_t LP5562_ENABLE_LOG_EN = 0x80;
static constexpr uint8_t LP5562_CONFIG_INT_CLK_EN = 0x01;
static constexpr uint8_t LP5562_LED_MAP_ALL_PWM_REGISTERS = 0x00;  // every channel driven by its PWM register

// PWM duty-cycle registers, indexed the same way as the LP5562_CHANNELS enum in output.py
static const uint8_t LP5562_REG_PWM[4] PROGMEM = {
    0x02,  // blue
    0x03,  // green
    0x04,  // red
    0x0e,  // white
};

// Current registers, same order
static const uint8_t LP5562_REG_CURRENT[4] PROGMEM = {
    0x05,  // blue
    0x06,  // green
    0x07,  // red
    0x0f,  // white
};

void LP5562Output::setup() {
  // clear any state left over from before a warm reboot; the chip may not ACK while resetting, so ignore the result
  this->write_byte(LP5562_REG_RESET, LP5562_RESET_VALUE);
  delay(1);
  // power up the chip and enable the internal 32 kHz oscillator, see
  // https://github.com/m5stack/M5GFX/blob/b2422cdb4735159209c2097d425c0756505b9a81/src/M5GFX.cpp#L566
  const uint8_t enable = LP5562_ENABLE_CHIP_EN | (this->logarithmic_dimming_ ? LP5562_ENABLE_LOG_EN : 0);
  if (!this->write_byte(LP5562_REG_ENABLE, enable)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  delay(1);
  // use the internal clock, and drive every channel directly from its PWM register instead of the
  // internal blink/fade engines
  if (!this->write_byte(LP5562_REG_CONFIG, LP5562_CONFIG_INT_CLK_EN) ||
      !this->write_byte(LP5562_REG_LED_MAP, LP5562_LED_MAP_ALL_PWM_REGISTERS)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  for (size_t channel = 0; channel < this->currents_.size(); channel++) {
    if (!this->write_byte(progmem_read_byte(&LP5562_REG_CURRENT[channel]), this->currents_[channel])) {
      this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
      return;
    }
  }
}

void LP5562Output::dump_config() {
  ESP_LOGCONFIG(TAG,
                "LP5562:\n"
                "  Logarithmic dimming: %s",
                YESNO(this->logarithmic_dimming_));
  LOG_I2C_DEVICE(this);
}

void LP5562Output::write_channel(uint8_t channel, float state) {
  if (this->is_failed())
    return;
  const uint8_t max_duty = 255;
  const auto duty = static_cast<uint8_t>(roundf(state * max_duty));
  this->status_comm_result(this->write_byte(progmem_read_byte(&LP5562_REG_PWM[channel]), duty));
}

}  // namespace esphome::lp5562

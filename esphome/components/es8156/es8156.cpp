#include "es8156.h"
#include "es8156_const.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace es8156 {

static const char *const TAG = "es8156";

// Mark the component as failed; use only in setup
#define ES8156_ERROR_FAILED(func) \
  if (!(func)) { \
    this->mark_failed(); \
    return; \
  }

void ES8156::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ES8156...");

  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG02_SCLK_MODE, 0x04));
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG20_ANALOG_SYS1, 0x2A));
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG21_ANALOG_SYS2, 0x3C));
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG22_ANALOG_SYS3, 0x00));
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG24_ANALOG_LP, 0x07));
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG23_ANALOG_SYS4, 0x00));

  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG0A_TIME_CONTROL1, 0x01));
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG0B_TIME_CONTROL2, 0x01));
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG11_DAC_SDP, 0x00));
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG19_EQ_CONTROL1, 0x20));

  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG0D_P2S_CONTROL, 0x14));
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG09_MISC_CONTROL2, 0x00));
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG18_MISC_CONTROL3, 0x00));
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG08_CLOCK_ON_OFF, 0x3F));
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG00_RESET, 0x02));
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG00_RESET, 0x03));
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG25_ANALOG_SYS5, 0x20));
}

void ES8156::dump_config() {
  ESP_LOGCONFIG(TAG, "ES8156 Audio Codec:");

  if (this->is_failed()) {
    ESP_LOGCONFIG(TAG, "  Failed to initialize");
    return;
  }
}

bool ES8156::set_volume(float volume) {
  volume = clamp(volume, 0.0f, 1.0f);
  uint8_t reg = remap<uint8_t, float>(volume, 0.0f, 1.0f, 0, 255);
  ESP_LOGV(TAG, "Setting ES8156_REG14_VOLUME_CONTROL to %u (volume: %f)", reg, volume);
  return this->write_byte(ES8156_REG14_VOLUME_CONTROL, reg);
}

float ES8156::volume() {
  uint8_t reg;
  this->read_byte(ES8156_REG14_VOLUME_CONTROL, &reg);
  return remap<float, uint8_t>(reg, 0, 255, 0.0f, 1.0f);
}

bool ES8156::set_mute_state_(bool mute_state) {
  uint8_t reg13;

  this->is_muted_ = mute_state;

  if (!this->read_byte(ES8156_REG13_DAC_MUTE, &reg13)) {
    return false;
  }

  ESP_LOGV(TAG, "Read ES8156_REG13_DAC_MUTE: %u", reg13);

  if (mute_state) {
    reg13 |= BIT(1) | BIT(2);
  } else {
    reg13 &= ~(BIT(1) | BIT(2));
  }

  ESP_LOGV(TAG, "Setting ES8156_REG13_DAC_MUTE to %u (muted: %s)", reg13, YESNO(mute_state));
  return this->write_byte(ES8156_REG13_DAC_MUTE, reg13);
}

}  // namespace es8156
}  // namespace esphome

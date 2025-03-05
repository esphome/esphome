#include "es7243e.h"
#include "es7243e_const.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome {
namespace es7243e {

static const char *const TAG = "es7243e";

// Mark the component as failed; use only in setup
#define ES7243E_ERROR_FAILED(func) \
  if (!(func)) { \
    this->mark_failed(); \
    return; \
  }

// Return false; use outside of setup
#define ES7243E_ERROR_CHECK(func) \
  if (!(func)) { \
    return false; \
  }

void ES7243E::dump_config() {
  ESP_LOGCONFIG(TAG, "ES7243E audio ADC:");

  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Failed to initialize");
    return;
  }
}

void ES7243E::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ES7243E...");

  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_CLOCK_MGR_REG01, 0x3A));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_RESET_REG00, 0x80));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_TEST_MODE_REGF9, 0x00));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_CLOCK_MGR_REG04, 0x02));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_CLOCK_MGR_REG04, 0x01));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_TEST_MODE_REGF9, 0x01));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_RESET_REG00, 0x1E));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_CLOCK_MGR_REG01, 0x00));

  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_CLOCK_MGR_REG02, 0x00));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_CLOCK_MGR_REG03, 0x20));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_CLOCK_MGR_REG04, 0x01));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_ADC_CTRL_REG0D, 0x00));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_CLOCK_MGR_REG05, 0x00));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_CLOCK_MGR_REG06, 0x03));  // SCLK=MCLK/4
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_CLOCK_MGR_REG07, 0x00));  // LRCK=MCLK/256
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_CLOCK_MGR_REG08, 0xFF));  // LRCK=MCLK/256

  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_CLOCK_MGR_REG09, 0xCA));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_SDP_REG0A, 0x85));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_SDP_REG0B, 0x00));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_ADC_CTRL_REG0E, 0xBF));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_ADC_CTRL_REG0F, 0x80));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_ADC_CTRL_REG14, 0x0C));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_ADC_CTRL_REG15, 0x0C));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_ANALOG_REG17, 0x02));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_ANALOG_REG18, 0x26));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_ANALOG_REG19, 0x77));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_ANALOG_REG1A, 0xF4));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_ANALOG_REG1B, 0x66));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_ANALOG_REG1C, 0x44));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_ANALOG_REG1E, 0x00));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_ANALOG_REG1F, 0x0C));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_ANALOG_REG20, 0x1A));  // PGA gain +30dB
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_ANALOG_REG21, 0x1A));

  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_RESET_REG00, 0x80));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_CLOCK_MGR_REG01, 0x3A));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_ANALOG_REG16, 0x3F));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_ANALOG_REG16, 0x00));

  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_TEST_MODE_REGF9, 0x00));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_CLOCK_MGR_REG04, 0x01));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_ANALOG_REG17, 0x01));
  ES7243E_ERROR_FAILED(this->configure_mic_gain_());
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_RESET_REG00, 0x80));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_CLOCK_MGR_REG01, 0x3A));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_ANALOG_REG16, 0x3F));
  ES7243E_ERROR_FAILED(this->write_byte(ES7243E_ANALOG_REG16, 0x00));

  this->setup_complete_ = true;
}

bool ES7243E::set_mic_gain(float mic_gain) {
  this->mic_gain_ = clamp<float>(mic_gain, 0, 37.5);
  if (this->setup_complete_) {
    return this->configure_mic_gain_();
  }
  return true;
}

bool ES7243E::configure_mic_gain_() {
  auto regv = this->es7243e_gain_reg_value_(this->mic_gain_);

  ES7243E_ERROR_CHECK(this->write_byte(ES7243E_ANALOG_REG20, 0x10 | regv));
  ES7243E_ERROR_CHECK(this->write_byte(ES7243E_ANALOG_REG21, 0x10 | regv));

  return true;
}

uint8_t ES7243E::es7243e_gain_reg_value_(float mic_gain) {
  // reg: 12 - 34.5dB, 13 - 36dB, 14 - 37.5dB
  mic_gain += 0.5;
  if (mic_gain <= 33.0) {
    return (uint8_t) mic_gain / 3;
  }
  if (mic_gain < 36.0) {
    return 12;
  }
  if (mic_gain < 37.0) {
    return 13;
  }
  return 14;
}

}  // namespace es7243e
}  // namespace esphome

#include "es7210.h"
#include "es7210_const.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace es7210 {

static const char *const TAG = "es7210";

static const size_t MCLK_DIV_FRE = 256;

// Mark the component as failed; use only in setup
#define ES7210_ERROR_FAILED(func) \
  if (!(func)) { \
    this->mark_failed(); \
    return; \
  }

// Return false; use outside of setup
#define ES7210_ERROR_CHECK(func) \
  if (!(func)) { \
    return false; \
  }

void ES7210::dump_config() {
  ESP_LOGCONFIG(TAG, "ES7210 audio ADC:");
  ESP_LOGCONFIG(TAG, "  Bits Per Sample: %" PRIu8, this->bits_per_sample_);
  ESP_LOGCONFIG(TAG, "  Sample Rate: %" PRIu32, this->sample_rate_);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Failed to initialize");
    return;
  }
}

void ES7210::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ES7210...");

  // Software reset
  ES7210_ERROR_FAILED(this->write_byte(ES7210_RESET_REG00, 0xff));
  ES7210_ERROR_FAILED(this->write_byte(ES7210_RESET_REG00, 0x32));
  ES7210_ERROR_FAILED(this->write_byte(ES7210_CLOCK_OFF_REG01, 0x3f));

  // Set initialization time when device powers up
  ES7210_ERROR_FAILED(this->write_byte(ES7210_TIME_CONTROL0_REG09, 0x30));
  ES7210_ERROR_FAILED(this->write_byte(ES7210_TIME_CONTROL1_REG0A, 0x30));

  // Configure HFP for all ADC channels
  ES7210_ERROR_FAILED(this->write_byte(ES7210_ADC12_HPF2_REG23, 0x2a));
  ES7210_ERROR_FAILED(this->write_byte(ES7210_ADC12_HPF1_REG22, 0x0a));
  ES7210_ERROR_FAILED(this->write_byte(ES7210_ADC34_HPF2_REG20, 0x0a));
  ES7210_ERROR_FAILED(this->write_byte(ES7210_ADC34_HPF1_REG21, 0x2a));

  // Secondary I2S mode settings
  ES7210_ERROR_FAILED(this->es7210_update_reg_bit_(ES7210_MODE_CONFIG_REG08, 0x01, 0x00));

  // Configure analog power
  ES7210_ERROR_FAILED(this->write_byte(ES7210_ANALOG_REG40, 0xC3));

  // Set mic bias
  ES7210_ERROR_FAILED(this->write_byte(ES7210_MIC12_BIAS_REG41, 0x70));
  ES7210_ERROR_FAILED(this->write_byte(ES7210_MIC34_BIAS_REG42, 0x70));

  // Configure I2S settings, sample rate, and microphone gains
  ES7210_ERROR_FAILED(this->configure_i2s_format_());
  ES7210_ERROR_FAILED(this->configure_sample_rate_());
  ES7210_ERROR_FAILED(this->configure_mic_gain_());

  // Power on mics 1 through 4
  ES7210_ERROR_FAILED(this->write_byte(ES7210_MIC1_POWER_REG47, 0x08));
  ES7210_ERROR_FAILED(this->write_byte(ES7210_MIC2_POWER_REG48, 0x08));
  ES7210_ERROR_FAILED(this->write_byte(ES7210_MIC3_POWER_REG49, 0x08));
  ES7210_ERROR_FAILED(this->write_byte(ES7210_MIC4_POWER_REG4A, 0x08));

  // Power down DLL
  ES7210_ERROR_FAILED(this->write_byte(ES7210_POWER_DOWN_REG06, 0x04));

  // Power on MIC1-4 bias & ADC1-4 & PGA1-4 Power
  ES7210_ERROR_FAILED(this->write_byte(ES7210_MIC12_POWER_REG4B, 0x0F));
  ES7210_ERROR_FAILED(this->write_byte(ES7210_MIC34_POWER_REG4C, 0x0F));

  // Enable device
  ES7210_ERROR_FAILED(this->write_byte(ES7210_RESET_REG00, 0x71));
  ES7210_ERROR_FAILED(this->write_byte(ES7210_RESET_REG00, 0x41));

  this->setup_complete_ = true;
}

bool ES7210::set_mic_gain(float mic_gain) {
  this->mic_gain_ = clamp<float>(mic_gain, ES7210_MIC_GAIN_MIN, ES7210_MIC_GAIN_MAX);
  if (this->setup_complete_) {
    return this->configure_mic_gain_();
  }
  return true;
}

bool ES7210::configure_sample_rate_() {
  int mclk_fre = this->sample_rate_ * MCLK_DIV_FRE;
  int coeff = -1;

  for (int i = 0; i < (sizeof(ES7210_COEFFICIENTS) / sizeof(ES7210_COEFFICIENTS[0])); ++i) {
    if (ES7210_COEFFICIENTS[i].lrclk == this->sample_rate_ && ES7210_COEFFICIENTS[i].mclk == mclk_fre)
      coeff = i;
  }

  if (coeff >= 0) {
    // Set adc_div & doubler & dll
    uint8_t regv;
    ES7210_ERROR_CHECK(this->read_byte(ES7210_MAINCLK_REG02, &regv));
    regv = regv & 0x00;
    regv |= ES7210_COEFFICIENTS[coeff].adc_div;
    regv |= ES7210_COEFFICIENTS[coeff].doubler << 6;
    regv |= ES7210_COEFFICIENTS[coeff].dll << 7;

    ES7210_ERROR_CHECK(this->write_byte(ES7210_MAINCLK_REG02, regv));

    // Set osr
    regv = ES7210_COEFFICIENTS[coeff].osr;
    ES7210_ERROR_CHECK(this->write_byte(ES7210_OSR_REG07, regv));
    // Set lrck
    regv = ES7210_COEFFICIENTS[coeff].lrck_h;
    ES7210_ERROR_CHECK(this->write_byte(ES7210_LRCK_DIVH_REG04, regv));
    regv = ES7210_COEFFICIENTS[coeff].lrck_l;
    ES7210_ERROR_CHECK(this->write_byte(ES7210_LRCK_DIVL_REG05, regv));
  } else {
    // Invalid sample frequency
    ESP_LOGE(TAG, "Invalid sample rate");
    return false;
  }

  return true;
}

bool ES7210::configure_mic_gain_() {
  auto regv = this->es7210_gain_reg_value_(this->mic_gain_);
  for (uint8_t i = 0; i < 4; ++i) {
    ES7210_ERROR_CHECK(this->es7210_update_reg_bit_(ES7210_MIC1_GAIN_REG43 + i, 0x10, 0x00));
  }
  ES7210_ERROR_CHECK(this->write_byte(ES7210_MIC12_POWER_REG4B, 0xff));
  ES7210_ERROR_CHECK(this->write_byte(ES7210_MIC34_POWER_REG4C, 0xff));

  // Configure mic 1
  ES7210_ERROR_CHECK(this->es7210_update_reg_bit_(ES7210_CLOCK_OFF_REG01, 0x0b, 0x00));
  ES7210_ERROR_CHECK(this->write_byte(ES7210_MIC12_POWER_REG4B, 0x00));
  ES7210_ERROR_CHECK(this->es7210_update_reg_bit_(ES7210_MIC1_GAIN_REG43, 0x10, 0x10));
  ES7210_ERROR_CHECK(this->es7210_update_reg_bit_(ES7210_MIC1_GAIN_REG43, 0x0f, regv));

  // Configure mic 2
  ES7210_ERROR_CHECK(this->es7210_update_reg_bit_(ES7210_CLOCK_OFF_REG01, 0x0b, 0x00));
  ES7210_ERROR_CHECK(this->write_byte(ES7210_MIC12_POWER_REG4B, 0x00));
  ES7210_ERROR_CHECK(this->es7210_update_reg_bit_(ES7210_MIC2_GAIN_REG44, 0x10, 0x10));
  ES7210_ERROR_CHECK(this->es7210_update_reg_bit_(ES7210_MIC2_GAIN_REG44, 0x0f, regv));

  // Configure mic 3
  ES7210_ERROR_CHECK(this->es7210_update_reg_bit_(ES7210_CLOCK_OFF_REG01, 0x0b, 0x00));
  ES7210_ERROR_CHECK(this->write_byte(ES7210_MIC34_POWER_REG4C, 0x00));
  ES7210_ERROR_CHECK(this->es7210_update_reg_bit_(ES7210_MIC3_GAIN_REG45, 0x10, 0x10));
  ES7210_ERROR_CHECK(this->es7210_update_reg_bit_(ES7210_MIC3_GAIN_REG45, 0x0f, regv));

  // Configure mic 4
  ES7210_ERROR_CHECK(this->es7210_update_reg_bit_(ES7210_CLOCK_OFF_REG01, 0x0b, 0x00));
  ES7210_ERROR_CHECK(this->write_byte(ES7210_MIC34_POWER_REG4C, 0x00));
  ES7210_ERROR_CHECK(this->es7210_update_reg_bit_(ES7210_MIC4_GAIN_REG46, 0x10, 0x10));
  ES7210_ERROR_CHECK(this->es7210_update_reg_bit_(ES7210_MIC4_GAIN_REG46, 0x0f, regv));

  return true;
}

uint8_t ES7210::es7210_gain_reg_value_(float mic_gain) {
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

bool ES7210::configure_i2s_format_() {
  // Configure bits per sample
  uint8_t reg_val = 0;
  switch (this->bits_per_sample_) {
    case ES7210_BITS_PER_SAMPLE_16:
      reg_val = 0x60;
      break;
    case ES7210_BITS_PER_SAMPLE_18:
      reg_val = 0x40;
      break;
    case ES7210_BITS_PER_SAMPLE_20:
      reg_val = 0x20;
      break;
    case ES7210_BITS_PER_SAMPLE_24:
      reg_val = 0x00;
      break;
    case ES7210_BITS_PER_SAMPLE_32:
      reg_val = 0x80;
      break;
    default:
      return false;
  }
  ES7210_ERROR_CHECK(this->write_byte(ES7210_SDP_INTERFACE1_REG11, reg_val));

  if (this->enable_tdm_) {
    ES7210_ERROR_CHECK(this->write_byte(ES7210_SDP_INTERFACE2_REG12, 0x02));
  } else {
    // Microphones 1 and 2 output on SDOUT1, microphones 3 and 4 output on SDOUT2
    ES7210_ERROR_CHECK(this->write_byte(ES7210_SDP_INTERFACE2_REG12, 0x00));
  }

  return true;
}

bool ES7210::es7210_update_reg_bit_(uint8_t reg_addr, uint8_t update_bits, uint8_t data) {
  uint8_t regv;
  ES7210_ERROR_CHECK(this->read_byte(reg_addr, &regv));
  regv = (regv & (~update_bits)) | (update_bits & data);
  return this->write_byte(reg_addr, regv);
}

}  // namespace es7210
}  // namespace esphome

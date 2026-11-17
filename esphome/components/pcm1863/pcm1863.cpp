#include "pcm1863.h"
#include "pcm1863_const.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include <cmath>

namespace esphome::pcm1863 {

ESPHOME_LOG_TAG(TAG, "pcm1863");

// Indexed by PCM1863BitsPerSample
PROGMEM_STRING_TABLE(BitsPerSampleStrings, "32", "24", "20", "16");
PROGMEM_STRING_TABLE(InputStrings, "none", "VIN1", "VIN2", "VIN3", "VIN4", "DIFF_1", "DIFF_2");

// Page select and power up: bits 2:0 clear is the run state, and the reserved bits 7:3
// must be written as 0b01110 or the analog section stops
static constexpr i2c::I2CRegValue PCM1863_INIT[] PROGMEM = {
    {PCM1863_REG_PAGE, PCM1863_PAGE_0},
    {PCM1863_REG_POWER_CTRL, PCM1863_PWR_CTRL_RSV},
};

void PCM1863::dump_config() {
  ESP_LOGCONFIG(TAG,
                "PCM1863 audio ADC:\n"
                "  Bits Per Sample: %s\n"
                "  Input: %s\n"
                "  Mic Gain: %.1f dB",
                LOG_STR_ARG(BitsPerSampleStrings::get_log_str(this->bits_per_sample_, 0)),
                LOG_STR_ARG(InputStrings::get_log_str(this->input_, 0)), this->mic_gain_);
#ifdef USE_PCM1863_SELECT
  LOG_SELECT("  ", "Input", this->input_select_);
#endif
}

void PCM1863::setup() {
  // Writing PCM1863_RESET to the page selector restores every register to its default
  if (!this->setup_check(this->write_byte(PCM1863_REG_PAGE, PCM1863_RESET)))
    return;
  delay(2);
  // The datasheet does not say which page the reset leaves selected, so select page 0 first
  if (!this->setup_check(this->write_registers(PCM1863_INIT)))
    return;

  if (!this->setup_check(this->configure_i2s_format_() && this->configure_input_() && this->configure_mic_gain_()))
    return;

  this->setup_complete_ = true;

#ifdef USE_PCM1863_SELECT
  // Show the input the ADC booted with, so the select does not start out blank
  PUBLISH_STATE(this->input_select_, this->input_);
#endif
}

bool PCM1863::set_mic_gain(float mic_gain) {
  this->mic_gain_ = clamp<float>(mic_gain, PCM1863_MIC_GAIN_MIN, PCM1863_MIC_GAIN_MAX);
  if (!this->setup_complete_)
    return true;
  return this->status_comm_result(this->configure_mic_gain_());
}

bool PCM1863::configure_mic_gain_() {
  // The PGA gain registers take a two's complement value in 0.5 dB steps
  const auto reg_val = static_cast<uint8_t>(lroundf(this->mic_gain_ / PCM1863_MIC_GAIN_STEP));
  return this->write_byte(PCM1863_REG_PGA_VAL_CH1_L, reg_val) && this->write_byte(PCM1863_REG_PGA_VAL_CH1_R, reg_val);
}

bool PCM1863::set_input(PCM1863Input input) {
  this->input_ = input;
  if (!this->setup_complete_)
    return true;
  return this->status_comm_result(this->configure_input_());
}

bool PCM1863::configure_input_() {
  // Bit 6 of both input select registers is reserved and must always be written as 1
  const uint8_t mux = this->input_ == PCM1863_INPUT_NONE ? 0 : 1 << (this->input_ - 1);
  const uint8_t reg_val = PCM1863_INPUT_SEL_RSV | mux;
  return this->write_byte(PCM1863_REG_ADC1_INPUT_SEL_L, reg_val) &&
         this->write_byte(PCM1863_REG_ADC1_INPUT_SEL_R, reg_val);
}

bool PCM1863::configure_i2s_format_() {
  const uint8_t wlen = this->bits_per_sample_;
  const uint8_t reg_val =
      (wlen << PCM1863_PCM_CFG_RX_WLEN_SHIFT) | (wlen << PCM1863_PCM_CFG_TX_WLEN_SHIFT) | PCM1863_PCM_CFG_FMT_I2S;
  return this->write_byte(PCM1863_REG_PCM_CFG, reg_val);
}

}  // namespace esphome::pcm1863

#include "pcm1863.h"
#include "pcm1863_const.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <array>
#include <cinttypes>
#include <cmath>

namespace esphome::pcm1863 {

ESPHOME_LOG_TAG(TAG, "pcm1863");

// The inputs a select platform offers, in the order PCM1863_INPUT_ENUM declares them in
// audio_adc.py: the select hands over the position of the chosen option, not its value.
static constexpr std::array<PCM1863Input, 7> INPUT_BY_INDEX = {
    PCM1863_INPUT_SEL_NONE, PCM1863_INPUT_SEL_VIN1,   PCM1863_INPUT_SEL_VIN2,   PCM1863_INPUT_SEL_VIN3,
    PCM1863_INPUT_SEL_VIN4, PCM1863_INPUT_SEL_DIFF_1, PCM1863_INPUT_SEL_DIFF_2,
};

static const LogString *input_to_str(PCM1863Input input) {
  switch (input) {
    case PCM1863_INPUT_SEL_NONE:
      return LOG_STR("none");
    case PCM1863_INPUT_SEL_VIN1:
      return LOG_STR("VIN1");
    case PCM1863_INPUT_SEL_VIN2:
      return LOG_STR("VIN2");
    case PCM1863_INPUT_SEL_VIN3:
      return LOG_STR("VIN3");
    case PCM1863_INPUT_SEL_VIN4:
      return LOG_STR("VIN4");
    case PCM1863_INPUT_SEL_DIFF_1:
      return LOG_STR("DIFF_1");
    case PCM1863_INPUT_SEL_DIFF_2:
      return LOG_STR("DIFF_2");
  }
  return LOG_STR("unknown");
}

void PCM1863::dump_config() {
  ESP_LOGCONFIG(TAG,
                "PCM1863 audio ADC:\n"
                "  Bits Per Sample: %" PRIu8 "\n"
                "  Input: %s\n"
                "  Mic Gain: %.1f dB",
                this->bits_per_sample_, LOG_STR_ARG(input_to_str(this->input_)), this->mic_gain_);
#ifdef USE_PCM1863_SELECT
  LOG_SELECT("  ", "Input", this->input_select_);
#endif
}

void PCM1863::setup() {
  // Writing PCM1863_RESET to the page selector restores every register to its default
  if (!this->setup_check(this->write_byte(PCM1863_REG_PAGE, PCM1863_RESET)))
    return;
  delay(2);
  // The datasheet documents the reset but not what it leaves the page selector at,
  // so select page 0 explicitly before writing any of the registers below
  if (!this->setup_check(this->write_byte(PCM1863_REG_PAGE, PCM1863_PAGE_0)))
    return;

  // Power up: bits 2:0 all clear is the run state. Bits 7:3 are reserved and the datasheet
  // requires them to always be written as 0b01110; zeroing them stops the analog section,
  // which leaves VREF to collapse and both ADCs stuck at a constant code.
  if (!this->setup_check(this->write_byte(PCM1863_REG_POWER_CTRL, PCM1863_PWR_CTRL_RSV)))
    return;

  if (!this->setup_check(this->configure_i2s_format_() && this->configure_input_() && this->configure_mic_gain_()))
    return;

  this->setup_complete_ = true;

#ifdef USE_PCM1863_SELECT
  // Show the input the ADC booted with, so the select does not start out blank
  if (this->input_select_ != nullptr) {
    for (size_t i = 0; i < INPUT_BY_INDEX.size(); i++) {
      if (INPUT_BY_INDEX[i] == this->input_) {
        this->input_select_->publish_state(i);
        break;
      }
    }
  }
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

bool PCM1863::set_input_index(size_t index) {
  if (index >= INPUT_BY_INDEX.size())
    return false;
  return this->set_input(INPUT_BY_INDEX[index]);
}

bool PCM1863::configure_input_() {
  // Bit 6 of both input select registers is reserved and must always be written as 1
  const uint8_t reg_val = PCM1863_INPUT_SEL_RSV | static_cast<uint8_t>(this->input_);
  return this->write_byte(PCM1863_REG_ADC1_INPUT_SEL_L, reg_val) &&
         this->write_byte(PCM1863_REG_ADC1_INPUT_SEL_R, reg_val);
}

bool PCM1863::configure_i2s_format_() {
  uint8_t wlen = PCM1863_PCM_CFG_WLEN_24;
  switch (this->bits_per_sample_) {
    case PCM1863_BITS_PER_SAMPLE_16:
      wlen = PCM1863_PCM_CFG_WLEN_16;
      break;
    case PCM1863_BITS_PER_SAMPLE_20:
      wlen = PCM1863_PCM_CFG_WLEN_20;
      break;
    case PCM1863_BITS_PER_SAMPLE_24:
      wlen = PCM1863_PCM_CFG_WLEN_24;
      break;
    case PCM1863_BITS_PER_SAMPLE_32:
      wlen = PCM1863_PCM_CFG_WLEN_32;
      break;
  }

  const uint8_t reg_val =
      (wlen << PCM1863_PCM_CFG_RX_WLEN_SHIFT) | (wlen << PCM1863_PCM_CFG_TX_WLEN_SHIFT) | PCM1863_PCM_CFG_FMT_I2S;
  return this->write_byte(PCM1863_REG_PCM_CFG, reg_val);
}

}  // namespace esphome::pcm1863

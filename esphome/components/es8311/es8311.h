#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"

namespace esphome {
namespace es8311 {

enum ES8311MicGain {
  ES8311_MIC_GAIN_MIN = -1,
  ES8311_MIC_GAIN_0DB,
  ES8311_MIC_GAIN_6DB,
  ES8311_MIC_GAIN_12DB,
  ES8311_MIC_GAIN_18DB,
  ES8311_MIC_GAIN_24DB,
  ES8311_MIC_GAIN_30DB,
  ES8311_MIC_GAIN_36DB,
  ES8311_MIC_GAIN_42DB,
  ES8311_MIC_GAIN_MAX
};

enum ES8311Resolution {
  ES8311_RESOLUTION_16 = 16,
  ES8311_RESOLUTION_18 = 18,
  ES8311_RESOLUTION_20 = 20,
  ES8311_RESOLUTION_24 = 24,
  ES8311_RESOLUTION_32 = 32
};

struct ES8311Coefficient {
  uint32_t mclk;     // mclk frequency
  uint32_t rate;     // sample rate
  uint8_t pre_div;   // the pre divider with range from 1 to 8
  uint8_t pre_mult;  // the pre multiplier with x1, x2, x4 and x8 selection
  uint8_t adc_div;   // adcclk divider
  uint8_t dac_div;   // dacclk divider
  uint8_t fs_mode;   // single speed (0) or double speed (1)
  uint8_t lrck_h;    // adc lrck divider and dac lrck divider
  uint8_t lrck_l;    //
  uint8_t bclk_div;  // sclk divider
  uint8_t adc_osr;   // adc osr
  uint8_t dac_osr;   // dac osr
};

class ES8311Component : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::LATE - 1; }
  void dump_config() override;

  void set_use_mclk(bool use_mclk) { this->use_mclk_ = use_mclk; }

  void set_volume(float volume);
  float get_volume();
  void set_mute(bool mute);

 protected:
  static const ES8311Coefficient *get_coefficient(uint32_t mclk, uint32_t rate);
  static uint8_t calculate_resolution_value(ES8311Resolution resolution);

  void configure_clock_();
  void configure_format_();
  void configure_microphone_();

  bool use_microphone_{false};
  ES8311MicGain microphone_gain_{ES8311_MIC_GAIN_42DB};

  bool use_mclk_{true};          // true = use dedicated MCLK pin, false = use SCLK
  bool sclk_inverted_{false};    // SCLK is inverted
  bool mclk_inverted_{false};    // MCLK is inverted (ignored if use_mclk_ == false)
  int mclk_frequency_{4096000};  // MCLK frequency (ignored if use_mclk_ == false)

  int sample_frequency_{16000};  // in Hz
  ES8311Resolution resolution_in_{ES8311_RESOLUTION_16};
  ES8311Resolution resolution_out_{ES8311_RESOLUTION_16};
};

}  // namespace es8311
}  // namespace esphome

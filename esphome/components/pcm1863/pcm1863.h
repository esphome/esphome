#pragma once

#include "esphome/components/audio_adc/audio_adc.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#ifdef USE_PCM1863_SELECT
#include "esphome/components/select/select.h"
#endif

#include "pcm1863_const.h"

namespace esphome::pcm1863 {

// Positions of the select options, in the order audio_adc.py declares them
enum PCM1863Input : uint8_t {
  PCM1863_INPUT_NONE,
  PCM1863_INPUT_VIN1,
  PCM1863_INPUT_VIN2,
  PCM1863_INPUT_VIN3,
  PCM1863_INPUT_VIN4,
  PCM1863_INPUT_DIFF_1,
  PCM1863_INPUT_DIFF_2,
};

// The word length code of PCM1863_REG_PCM_CFG
enum PCM1863BitsPerSample : uint8_t {
  PCM1863_BITS_PER_SAMPLE_32,
  PCM1863_BITS_PER_SAMPLE_24,
  PCM1863_BITS_PER_SAMPLE_20,
  PCM1863_BITS_PER_SAMPLE_16,
};

/// Texas Instruments PCM1863 stereo audio ADC for line and microphone input.
/// The register map and reset sequence follow the datasheet (SLAS831D) and the Linux
/// kernel driver in sound/soc/codecs/pcm186x.c.
class PCM1863 final : public audio_adc::AudioAdc, public Component, public i2c::I2CDevice {
#ifdef USE_PCM1863_SELECT
  SUB_SELECT(input)
#endif

 public:
  void setup() override;
  void dump_config() override;

  void set_bits_per_sample(PCM1863BitsPerSample bits_per_sample) { this->bits_per_sample_ = bits_per_sample; }

  /// Selects the analog input. Takes effect immediately once setup has run.
  bool set_input(PCM1863Input input);

  bool set_mic_gain(float mic_gain) override;
  float mic_gain() override { return this->mic_gain_; }

 protected:
  bool configure_i2s_format_();
  bool configure_mic_gain_();
  bool configure_input_();

  float mic_gain_{0};
  bool setup_complete_{false};
  PCM1863BitsPerSample bits_per_sample_{PCM1863_BITS_PER_SAMPLE_24};
  PCM1863Input input_{PCM1863_INPUT_VIN1};
};

}  // namespace esphome::pcm1863

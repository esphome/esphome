#pragma once

#include "esphome/components/audio_dac/audio_dac.h"
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

enum ES8311Resolution : uint8_t {
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

class ES8311 : public audio_dac::AudioDac, public Component, public i2c::I2CDevice {
 public:
  /////////////////////////
  // Component overrides //
  /////////////////////////

  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void dump_config() override;

  ////////////////////////
  // AudioDac overrides //
  ////////////////////////

  /// @brief Writes the volume out to the DAC
  /// @param volume floating point between 0.0 and 1.0
  /// @return True if successful and false otherwise
  bool set_volume(float volume) override;

  /// @brief Gets the current volume out from the DAC
  /// @return floating point between 0.0 and 1.0
  float volume() override;

  /// @brief Disables mute for audio out
  /// @return True if successful and false otherwise
  bool set_mute_off() override { return this->set_mute_state_(false); }

  /// @brief Enables mute for audio out
  /// @return True if successful and false otherwise
  bool set_mute_on() override { return this->set_mute_state_(true); }

  bool is_muted() override { return this->is_muted_; }

  //////////////////////////////////
  // ES8311 configuration setters //
  //////////////////////////////////

  void set_use_mclk(bool use_mclk) { this->use_mclk_ = use_mclk; }
  void set_bits_per_sample(ES8311Resolution resolution) {
    this->resolution_in_ = resolution;
    this->resolution_out_ = resolution;
  }
  void set_sample_frequency(uint32_t sample_frequency) { this->sample_frequency_ = sample_frequency; }
  void set_use_mic(bool use_mic) { this->use_mic_ = use_mic; }
  void set_mic_gain(ES8311MicGain mic_gain) { this->mic_gain_ = mic_gain; }

 protected:
  /// @brief Computes the register value for the configured resolution (bits per sample)
  /// @param resolution bits per sample enum for both audio in and audio out
  /// @return register value
  static uint8_t calculate_resolution_value(ES8311Resolution resolution);

  /// @brief Retrieves the appropriate registers values for the configured mclk and rate
  /// @param mclk mlck frequency in Hz
  /// @param rate sample rate frequency in Hz
  /// @return ES8311Coeffecient containing appropriate register values to configure the ES8311 or nullptr if impossible
  static const ES8311Coefficient *get_coefficient(uint32_t mclk, uint32_t rate);

  /// @brief Configures the ES8311 registers for the chosen sample rate
  /// @return True if successful and false otherwise
  bool configure_clock_();

  /// @brief Configures the ES8311 registers for the chosen bits per sample
  /// @return True if successful and false otherwise
  bool configure_format_();

  /// @brief Configures the ES8311 microphone registers
  /// @return True if successful and false otherwise
  bool configure_mic_();

  /// @brief Mutes or unmute the DAC audio out
  /// @param mute_state True to mute, false to unmute
  /// @return
  bool set_mute_state_(bool mute_state);

  bool use_mic_;
  ES8311MicGain mic_gain_;

  bool use_mclk_;                // true = use dedicated MCLK pin, false = use SCLK
  bool sclk_inverted_{false};    // SCLK is inverted
  bool mclk_inverted_{false};    // MCLK is inverted (ignored if use_mclk_ == false)
  uint32_t mclk_multiple_{256};  // MCLK frequency is sample rate * mclk_multiple_ (ignored if use_mclk_ == false)

  uint32_t sample_frequency_;  // in Hz
  ES8311Resolution resolution_in_;
  ES8311Resolution resolution_out_;
};

}  // namespace es8311
}  // namespace esphome

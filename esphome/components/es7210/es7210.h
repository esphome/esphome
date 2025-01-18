#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"

namespace esphome {
namespace es7210 {

enum ES7210BitsPerSample : uint8_t {
  ES7210_BITS_PER_SAMPLE_16 = 16,
  ES7210_BITS_PER_SAMPLE_18 = 18,
  ES7210_BITS_PER_SAMPLE_20 = 20,
  ES7210_BITS_PER_SAMPLE_24 = 24,
  ES7210_BITS_PER_SAMPLE_32 = 32,
};

enum ES7210MicGain : uint8_t {
  ES7210_MIC_GAIN_0DB = 0,
  ES7210_MIC_GAIN_3DB,
  ES7210_MIC_GAIN_6DB,
  ES7210_MIC_GAIN_9DB,
  ES7210_MIC_GAIN_12DB,
  ES7210_MIC_GAIN_15DB,
  ES7210_MIC_GAIN_18DB,
  ES7210_MIC_GAIN_21DB,
  ES7210_MIC_GAIN_24DB,
  ES7210_MIC_GAIN_27DB,
  ES7210_MIC_GAIN_30DB,
  ES7210_MIC_GAIN_33DB,
  ES7210_MIC_GAIN_34_5DB,
  ES7210_MIC_GAIN_36DB,
  ES7210_MIC_GAIN_37_5DB,
};

class ES7210 : public Component, public i2c::I2CDevice {
  /* Class for configuring an ES7210 ADC for microphone input.
   * Based on code from:
   * - https://github.com/espressif/esp-bsp/ (accessed 20241219)
   * - https://github.com/espressif/esp-adf/ (accessed 20241219)
   */
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void dump_config() override;

  void set_bits_per_sample(ES7210BitsPerSample bits_per_sample) { this->bits_per_sample_ = bits_per_sample; }
  void set_mic_gain(ES7210MicGain mic_gain) { this->mic_gain_ = mic_gain; }
  void set_sample_rate(uint32_t sample_rate) { this->sample_rate_ = sample_rate; }

 protected:
  /// @brief Updates an I2C registry address by modifying the current state
  /// @param reg_addr I2C register address
  /// @param update_bits Mask of allowed bits to be modified
  /// @param data Bit values to be written
  /// @return True if successful, false otherwise
  bool es7210_update_reg_bit_(uint8_t reg_addr, uint8_t update_bits, uint8_t data);

  bool configure_i2s_format_();
  bool configure_mic_gain_();
  bool configure_sample_rate_();

  bool enable_tdm_{false};  // TDM is unsupported in ESPHome as of version 2024.12
  ES7210MicGain mic_gain_;
  ES7210BitsPerSample bits_per_sample_;
  uint32_t sample_rate_;
};

}  // namespace es7210
}  // namespace esphome

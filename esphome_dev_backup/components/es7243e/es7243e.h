#pragma once

#include "esphome/components/audio_adc/audio_adc.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"

namespace esphome {
namespace es7243e {

class ES7243E : public audio_adc::AudioAdc, public Component, public i2c::I2CDevice {
  /* Class for configuring an ES7243E ADC for microphone input.
   * Based on code from:
   * - https://github.com/espressif/esp-adf/ (accessed 20250116)
   */
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void dump_config() override;

  bool set_mic_gain(float mic_gain) override;

  float mic_gain() override { return this->mic_gain_; };

 protected:
  /// @brief Convert floating point mic gain value to register value
  /// @param mic_gain Gain value to convert
  /// @return Corresponding register value for specified gain
  uint8_t es7243e_gain_reg_value_(float mic_gain);

  bool configure_mic_gain_();

  bool setup_complete_{false};
  float mic_gain_{0};
};

}  // namespace es7243e
}  // namespace esphome

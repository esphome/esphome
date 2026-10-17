#pragma once

#ifdef USE_ESP32

#include "esphome/components/audio_adc/audio_adc.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/i2s_audio/i2s_audio.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

namespace esphome::es7243 {

/// ES7243 microphone ADC. Not the ES7243E, which has a different register map and its own platform.
/// Register sequence from the ESP-ADF es7243 driver.
class ES7243 final : public audio_adc::AudioAdc, public Component, public i2c::I2CDevice {
 public:
  explicit ES7243(const i2s_audio::I2SAudioComponent *i2s) : i2s_(i2s) {}

  void setup() override;
  void dump_config() override;

  bool set_mic_gain(float mic_gain) override;
  float mic_gain() override { return this->mic_gain_; }

 protected:
  bool write_mic_gain_();

  const i2s_audio::I2SAudioComponent *i2s_;
  float mic_gain_{0};
  bool setup_complete_{false};
};

}  // namespace esphome::es7243

#endif  // USE_ESP32

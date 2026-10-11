#pragma once

#include <cstdint>

#include "esphome/components/audio_adc/audio_adc.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome::es7243 {

// Driver for the real ES7243 mic ADC (not ES7243E -- see audio_adc.py for
// why these need separate drivers). Register sequence and MCLK
// pre-activation ported from Espressif's own public driver:
// esp-adf/components/audio_hal/driver/es7243/es7243.c
class ES7243 final : public audio_adc::AudioAdc, public Component, public i2c::I2CDevice {
 public:
  explicit ES7243(uint8_t mclk_gpio_num) : mclk_gpio_num_(mclk_gpio_num) {}

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::IO; }

  bool set_mic_gain(float mic_gain) override;
  float mic_gain() override { return this->mic_gain_; }

 protected:
  // The chip's I2C interface doesn't respond reliably until it has seen some
  // MCLK activity, so this bit-bangs the pin as a plain GPIO output for a
  // short pulse train before any I2C traffic, exactly as ESP-ADF's own
  // es7243_mclk_active() does. Ownership of the pin is handed back to
  // whatever else drives it afterward (this component's setup_priority::IO
  // runs early enough that a subsequent i2s_audio setup won't conflict).
  void prime_mclk_();

  const uint8_t mclk_gpio_num_;
  float mic_gain_{0};
};

}  // namespace esphome::es7243

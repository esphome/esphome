#pragma once

#ifdef USE_ESP32

#include "../i2s_audio.h"

#include "esphome/components/microphone/microphone.h"
#include "esphome/core/component.h"

namespace esphome {
namespace i2s_audio {

class I2SAudioMicrophone : public I2SAudioIn, public microphone::Microphone, public Component {
 public:
  void setup() override;
  void start() override;
  void stop() override;

  void loop() override;
#ifdef USE_I2S_LEGACY
  void set_din_pin(int8_t pin) { this->din_pin_ = pin; }
#else
  void set_din_pin(int8_t pin) { this->din_pin_ = (gpio_num_t) pin; }
#endif

  void set_pdm(bool pdm) { this->pdm_ = pdm; }

  size_t read(int16_t *buf, size_t len) override;

#ifdef USE_I2S_LEGACY
#if SOC_I2S_SUPPORTS_ADC
  void set_adc_channel(adc1_channel_t channel) {
    this->adc_channel_ = channel;
    this->adc_ = true;
  }
#endif
#endif

 protected:
  void start_();
  void stop_();
  void read_();

#ifdef USE_I2S_LEGACY
  int8_t din_pin_{I2S_PIN_NO_CHANGE};
#if SOC_I2S_SUPPORTS_ADC
  adc1_channel_t adc_channel_{ADC1_CHANNEL_MAX};
  bool adc_{false};
#endif
#else
  gpio_num_t din_pin_{I2S_GPIO_UNUSED};
  i2s_chan_handle_t rx_handle_;
#endif
  bool pdm_{false};

  HighFrequencyLoopRequester high_freq_;
};

}  // namespace i2s_audio
}  // namespace esphome

#endif  // USE_ESP32

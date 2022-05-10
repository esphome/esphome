#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <Audio.h>

namespace esphome {
namespace audio {

class AudioComponent : public Component {
 public:
  void setup() override;
  float get_setup_priority() const override { return esphome::setup_priority::LATE; }

  void loop() override;

  void stop();

  void play_url(const std::string &url);

  void set_volume(int volume) {
    this->audio_->setVolume(clamp(volume, 0, 21));  // Use value of 0...21
  }

  void dump_config() override;

  void set_dout_pin(uint8_t pin) { this->dout_pin_ = pin; }
  void set_bclk_pin(uint8_t pin) { this->bclk_pin_ = pin; }
  void set_ws_pin(uint8_t pin) { this->ws_pin_ = pin; }

  void set_internal_dac_mode(i2s_dac_mode_t mode) { this->internal_dac_mode_ = mode; }

 protected:
  Audio *audio_{nullptr};

  uint8_t dout_pin_{0};
  uint8_t din_pin_{0};
  uint8_t bclk_pin_;
  uint8_t ws_pin_;

  i2s_dac_mode_t internal_dac_mode_{I2S_DAC_CHANNEL_DISABLE};

  HighFrequencyLoopRequester high_freq_;
};

}  // namespace audio
}  // namespace esphome

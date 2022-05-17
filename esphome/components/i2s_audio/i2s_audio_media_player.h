#pragma once

#include "esphome/components/media_player/media_player.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"

#include <Audio.h>

namespace esphome {
namespace i2s_audio {

class I2SAudioMediaPlayer : public Component, public media_player::MediaPlayer {
 public:
  void setup() override;
  float get_setup_priority() const override { return esphome::setup_priority::LATE; }

  void loop() override;

  void dump_config() override;

  void set_dout_pin(uint8_t pin) { this->dout_pin_ = pin; }
  void set_bclk_pin(uint8_t pin) { this->bclk_pin_ = pin; }
  void set_ws_pin(uint8_t pin) { this->ws_pin_ = pin; }
  void set_mute_pin(GPIOPin *mute_pin) { this->mute_pin_ = mute_pin; }
  void set_internal_dac_mode(i2s_dac_mode_t mode) { this->internal_dac_mode_ = mode; }

  media_player::MediaPlayerTraits get_traits() override;

  bool is_muted() const override { return this->muted_; }

 protected:
  void control(const media_player::MediaPlayerCall &call) override;

  void mute_();
  void unmute_();
  void set_volume_(float volume, bool publish = true);
  void stop_();

  Audio *audio_{nullptr};

  uint8_t dout_pin_{0};
  uint8_t din_pin_{0};
  uint8_t bclk_pin_;
  uint8_t ws_pin_;

  GPIOPin *mute_pin_{nullptr};
  bool muted_{false};
  float unmuted_volume_{0};

  i2s_dac_mode_t internal_dac_mode_{I2S_DAC_CHANNEL_DISABLE};

  HighFrequencyLoopRequester high_freq_;
};

}  // namespace i2s_audio
}  // namespace esphome

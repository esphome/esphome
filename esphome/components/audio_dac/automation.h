#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "audio_dac.h"

namespace esphome {
namespace audio_dac {

template<typename... Ts> class MuteOffAction : public Action<Ts...> {
 public:
  explicit MuteOffAction(AudioDac *audio_dac) : audio_dac_(audio_dac) {}

  void play(Ts... x) override { this->audio_dac_->set_mute_off(); }

 protected:
  AudioDac *audio_dac_;
};

template<typename... Ts> class MuteOnAction : public Action<Ts...> {
 public:
  explicit MuteOnAction(AudioDac *audio_dac) : audio_dac_(audio_dac) {}

  void play(Ts... x) override { this->audio_dac_->set_mute_on(); }

 protected:
  AudioDac *audio_dac_;
};

template<typename... Ts> class SetVolumeAction : public Action<Ts...> {
 public:
  explicit SetVolumeAction(AudioDac *audio_dac) : audio_dac_(audio_dac) {}

  TEMPLATABLE_VALUE(float, volume)

  void play(Ts... x) override { this->audio_dac_->set_volume(this->volume_.value(x...)); }

 protected:
  AudioDac *audio_dac_;
};

}  // namespace audio_dac
}  // namespace esphome

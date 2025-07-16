#pragma once

#include "speaker_media_player.h"

#ifdef USE_ESP_IDF

#include "esphome/components/audio/audio.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace speaker {

template<typename... Ts> class PlayOnDeviceMediaAction : public Action<Ts...>, public Parented<SpeakerMediaPlayer> {
  TEMPLATABLE_VALUE(audio::AudioFile *, audio_file)
  TEMPLATABLE_VALUE(bool, announcement)
  TEMPLATABLE_VALUE(bool, enqueue)
  void play(Ts... x) override {
    this->parent_->play_file(this->audio_file_.value(x...), this->announcement_.value(x...),
                             this->enqueue_.value(x...));
  }
};

}  // namespace speaker
}  // namespace esphome

#endif

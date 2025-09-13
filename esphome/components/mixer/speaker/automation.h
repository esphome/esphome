#pragma once

#include "mixer_speaker.h"

#ifdef USE_ESP32

namespace esphome {
namespace mixer_speaker {
template<typename... Ts> class DuckingApplyAction : public Action<Ts...>, public Parented<SourceSpeaker> {
  TEMPLATABLE_VALUE(uint8_t, decibel_reduction)
  TEMPLATABLE_VALUE(uint32_t, duration)
  void play(Ts... x) override {
    this->parent_->apply_ducking(this->decibel_reduction_.value(x...), this->duration_.value(x...));
  }
};
}  // namespace mixer_speaker
}  // namespace esphome

#endif

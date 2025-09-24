#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "audio_adc.h"

namespace esphome {
namespace audio_adc {

template<typename... Ts> class SetMicGainAction : public Action<Ts...> {
 public:
  explicit SetMicGainAction(AudioAdc *audio_adc) : audio_adc_(audio_adc) {}

  TEMPLATABLE_VALUE(float, mic_gain)

  void play(Ts... x) override { this->audio_adc_->set_mic_gain(this->mic_gain_.value(x...)); }

 protected:
  AudioAdc *audio_adc_;
};

}  // namespace audio_adc
}  // namespace esphome

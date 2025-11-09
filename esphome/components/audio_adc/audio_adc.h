#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace audio_adc {

class AudioAdc {
 public:
  virtual bool set_mic_gain(float mic_gain) = 0;

  virtual float mic_gain() = 0;
};

}  // namespace audio_adc
}  // namespace esphome

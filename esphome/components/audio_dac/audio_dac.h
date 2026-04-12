#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace audio_dac {

class AudioDac {
 public:
  virtual bool set_mute_off() = 0;
  virtual bool set_mute_on() = 0;
  virtual bool set_volume(float volume) = 0;

  virtual bool is_muted() = 0;
  virtual float volume() = 0;

 protected:
  bool is_muted_{false};
};

}  // namespace audio_dac
}  // namespace esphome

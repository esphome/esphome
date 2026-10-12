#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/hal.h"

namespace esphome::audio_dac {

class AudioDac {
 public:
  virtual bool set_mute_off() = 0;
  virtual bool set_mute_on() = 0;
  virtual bool set_volume(float volume) = 0;

  virtual bool is_muted() = 0;
  virtual float volume() = 0;

  /// Called from the main loop by the speaker driving this DAC once its audio clocks are running.
  /// DACs that can only be configured while clocked (e.g. DSP state lost across clock stops) override this.
  virtual void on_audio_started() {}

 protected:
  bool is_muted_{false};
};

}  // namespace esphome::audio_dac

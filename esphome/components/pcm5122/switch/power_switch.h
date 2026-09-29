#pragma once

#include "esphome/components/switch/switch.h"

#include "../pcm5122.h"

namespace esphome::pcm5122 {

enum PCM5122PowerSwitchMode : uint8_t {
  PCM5122_POWER_SWITCH_MODE_STANDBY,
  PCM5122_POWER_SWITCH_MODE_POWERDOWN,
};

class PCM5122PowerSwitch final : public switch_::Switch, public Parented<PCM5122> {
 public:
  void set_power_mode(PCM5122PowerSwitchMode mode) { this->mode_ = mode; }

 protected:
  void write_state(bool state) override;

  PCM5122PowerSwitchMode mode_{PCM5122_POWER_SWITCH_MODE_POWERDOWN};
};

}  // namespace esphome::pcm5122

#pragma once

#include "esphome/components/switch/switch.h"
#include "../airton.h"

namespace esphome::airton {

class SleepSwitch : public switch_::Switch, public Parented<AirtonClimate> {
 public:
  SleepSwitch() = default;

 protected:
  void write_state(bool state) override;
};

}  // namespace esphome::airton

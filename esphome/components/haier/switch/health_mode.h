#pragma once

#include "esphome/components/switch/switch.h"
#include "../haier_base.h"

namespace esphome::haier {

class HealthModeSwitch : public switch_::Switch, public Parented<HaierClimateBase> {
 public:
  HealthModeSwitch() = default;

 protected:
  void write_state(bool state) override;
};

}  // namespace esphome::haier

#pragma once

#include "esphome/components/switch/switch.h"
#include "../hon_climate.h"

namespace esphome::haier {

class BeeperSwitch final : public switch_::Switch, public Parented<HonClimate> {
 public:
  BeeperSwitch() = default;

 protected:
  void write_state(bool state) override;
};

}  // namespace esphome::haier

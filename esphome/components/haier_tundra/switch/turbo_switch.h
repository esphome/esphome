#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/haier_tundra/haier_tundra.h"

namespace esphome::haier_tundra {

class TurboSwitch final : public switch_::Switch, public Component, public Parented<HaierTundra> {
 public:
  void setup() override;
  void dump_config() override;

 protected:
  void write_state(bool state) override;
};

}  // namespace esphome::haier_tundra

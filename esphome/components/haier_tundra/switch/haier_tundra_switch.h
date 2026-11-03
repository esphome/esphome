#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/haier_tundra/haier_tundra.h"

namespace esphome::haier_tundra {

enum class SwitchKind : uint8_t {
  SWITCH_KIND_HEALTH,
  SWITCH_KIND_QUIET,
  SWITCH_KIND_TURBO,
};

class HaierTundraSwitch final : public switch_::Switch, public Component, public Parented<HaierTundra> {
 public:
  HaierTundraSwitch(HaierTundra *parent, SwitchKind kind) : Parented(parent), kind_(kind) {}
  void setup() override;
  void dump_config() override;

 protected:
  void write_state(bool state) override;
  void apply_(bool state, bool transmit);

  SwitchKind kind_;
};

}  // namespace esphome::haier_tundra

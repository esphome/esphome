#pragma once

#include "esphome/components/switch/switch.h"
#include "../ld2412.h"

namespace esphome::ld2412 {

class EngineeringModeSwitch final : public switch_::Switch, public Parented<LD2412Component> {
 public:
  // User provided, not "= default": `new(p) EngineeringModeSwitch()` would zero-fill .bss that is already zero.
  EngineeringModeSwitch() {}

 protected:
  void write_state(bool state) override;
};

}  // namespace esphome::ld2412

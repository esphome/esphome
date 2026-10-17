#pragma once

#include "esphome/components/select/select.h"
#include "../dfrobot_sen0609.h"

namespace esphome::dfrobot_sen0609 {

class OutPinLevelSelect final : public select::Select, public Parented<DFRobotSEN0609Component> {
 public:
  // User provided, not "= default": `new(p) OutPinLevelSelect()` would zero-fill .bss that is already zero.
  OutPinLevelSelect() {}

 protected:
  void control(size_t index) override { this->parent_->control_out_pin_level(index); }
};

}  // namespace esphome::dfrobot_sen0609

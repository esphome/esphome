#pragma once

#include "esphome/components/switch/switch.h"
#include "../dfrobot_sen0609.h"

namespace esphome::dfrobot_sen0609 {

class DFRobotSEN0609Switch final : public switch_::Switch, public Parented<DFRobotSEN0609Component> {
 public:
  explicit DFRobotSEN0609Switch(SwitchSetting setting) : setting_(setting) {}

 protected:
  void write_state(bool state) override { this->parent_->control_switch(this->setting_, state); }

  SwitchSetting setting_;
};

}  // namespace esphome::dfrobot_sen0609

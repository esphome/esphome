#pragma once

#include "esphome/components/number/number.h"
#include "../dfrobot_sen0609.h"

namespace esphome::dfrobot_sen0609 {

class DFRobotSEN0609Number final : public number::Number, public Parented<DFRobotSEN0609Component> {
 public:
  explicit DFRobotSEN0609Number(NumberSetting setting) : setting_(setting) {}

 protected:
  void control(float value) override { this->parent_->control_number(this->setting_, value); }

  NumberSetting setting_;
};

}  // namespace esphome::dfrobot_sen0609

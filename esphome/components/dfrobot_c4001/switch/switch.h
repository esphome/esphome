#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

#include "../dfrobot_c4001.h"

namespace esphome {
namespace dfrobot_c4001 {
class OutLedSwitch : public switch_::Switch, public Component, public Parented<DFRobotC4001Hub> {
 public:
  void write_state(bool state) override;
};
class RunLedSwitch : public switch_::Switch, public Component, public Parented<DFRobotC4001Hub> {
 public:
  void write_state(bool state) override;
};
class MicroMotionSwitch : public switch_::Switch, public Component, public Parented<DFRobotC4001Hub> {
 public:
  void write_state(bool state) override;
};
}  // namespace dfrobot_c4001
}  // namespace esphome

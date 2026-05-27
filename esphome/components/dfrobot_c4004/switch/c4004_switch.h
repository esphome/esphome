#pragma once

#include "../dfrobot_c4004.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace dfrobot_c4004 {

class C4004PresenceEnableSwitch : public switch_::Switch, public Parented<C4004Component> {
 protected:
  void write_state(bool state) override;
};

class C4004TrajectoryTrackingSwitch : public switch_::Switch, public Parented<C4004Component> {
 protected:
  void write_state(bool state) override;
};

class C4004TrajectoryLedSwitch : public switch_::Switch, public Parented<C4004Component> {
 protected:
  void write_state(bool state) override;
};

class C4004MotionLedSwitch : public switch_::Switch, public Parented<C4004Component> {
 protected:
  void write_state(bool state) override;
};

}  // namespace dfrobot_c4004
}  // namespace esphome

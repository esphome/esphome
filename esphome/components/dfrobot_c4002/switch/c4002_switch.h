#pragma once

#include "esphome/components/switch/switch.h"
#include "../dfrobot_c4002.h"

namespace esphome {
namespace dfrobot_c4002 {

class C4002Switch1 : public switch_::Switch, public Parented<C4002Component> {
 protected:
  void write_state(bool state) override;
};

class C4002Switch2 : public switch_::Switch, public Parented<C4002Component> {
 protected:
  void write_state(bool state) override;
};

class C4002SwitchFactoryReset : public switch_::Switch, public Component, public Parented<C4002Component> {
 protected:
  void write_state(bool state) override;
};

class C4002SwitchEnvironmentalCalibration : public switch_::Switch, public Component, public Parented<C4002Component> {
 protected:
  void write_state(bool state) override;
};

}  // namespace dfrobot_c4002
}  // namespace esphome

#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

#include "../dfrobot_sen0395.h"

namespace esphome {
namespace dfrobot_sen0395 {

class DfrobotSen0395Switch : public switch_::Switch, public Component, public Parented<DfrobotSen0395Component> {};

class Sen0395PowerSwitch : public DfrobotSen0395Switch {
 public:
  void write_state(bool state) override;
};

class Sen0395LedSwitch : public DfrobotSen0395Switch {
 public:
  void write_state(bool state) override;
};

class Sen0395UartPresenceSwitch : public DfrobotSen0395Switch {
 public:
  void write_state(bool state) override;
};

class Sen0395StartAfterBootSwitch : public DfrobotSen0395Switch {
 public:
  void write_state(bool state) override;
};

}  // namespace dfrobot_sen0395
}  // namespace esphome

#pragma once

#include "esphome/core/helpers.h"
#include "esphome/components/switch/switch.h"

#include "../ds3231.h"

namespace esphome::ds3231 {

class DS3231Enable32kHzSwitch : public switch_::Switch, public Parented<DS3231Component> {
 protected:
  void write_state(bool state) override;
};

/// Enables or disables one of the two alarm interrupts (A1IE / A2IE). Turning it off leaves the
/// programmed alarm time in place, so the alarm can be re-armed without reprogramming it. The
/// hub republishes the real chip state on every poll.
class DS3231AlarmSwitch : public switch_::Switch, public Parented<DS3231Component> {
 public:
  void set_alarm(uint8_t alarm) { this->alarm_ = alarm; }

 protected:
  void write_state(bool state) override;
  uint8_t alarm_{1};
};

}  // namespace esphome::ds3231

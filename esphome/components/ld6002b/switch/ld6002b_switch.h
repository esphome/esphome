#pragma once

#include "esphome/components/switch/switch.h"
#include "../ld6002b.h"

namespace esphome::ld6002b {

class LD6002BSwitch : public switch_::Switch, public Parented<LD6002BComponent> {
 public:
  explicit LD6002BSwitch(SwitchType type) : type_(type) {}

 protected:
  void write_state(bool state) override;

  SwitchType type_;
};

}  // namespace esphome::ld6002b

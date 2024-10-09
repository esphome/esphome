#pragma once

#include "esphome/components/switch/switch.h"
#include "../qn8027.h"

namespace esphome {
namespace qn8027 {

class MuteSwitch : public switch_::Switch, public Parented<QN8027Component> {
 public:
  MuteSwitch() = default;

 protected:
  void write_state(bool value) override;
};

}  // namespace qn8027
}  // namespace esphome

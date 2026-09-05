#pragma once

#include "comp_climate.h"
#include "esphome/components/switch/switch.h"

namespace esphome::sharp_ac {

class IonSwitch : public switch_::Switch, public Parented<SharpAc> {
 protected:
  void write_state(bool state) override { this->parent_->set_ion(state); }
};

}  // namespace esphome::sharp_ac

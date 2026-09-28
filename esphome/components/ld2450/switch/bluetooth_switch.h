#pragma once

#include "esphome/components/switch/switch.h"
#include "../ld2450.h"

namespace esphome::ld2450 {

class BluetoothSwitch : public switch_::Switch, public Parented<LD2450Component> {
 public:
  // User provided, not "= default": `new(p) BluetoothSwitch()` would zero-fill .bss that is already zero.
  BluetoothSwitch() {}

 protected:
  void write_state(bool state) override;
};

}  // namespace esphome::ld2450

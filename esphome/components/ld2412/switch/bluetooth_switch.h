#pragma once

#include "esphome/components/switch/switch.h"
#include "../ld2412.h"

namespace esphome::ld2412 {

class BluetoothSwitch final : public switch_::Switch, public Parented<LD2412Component> {
 public:
  // User provided, not "= default": `new(p) BluetoothSwitch()` would zero-fill .bss that is already zero.
  BluetoothSwitch() {}

 protected:
  void write_state(bool state) override;
};

}  // namespace esphome::ld2412

#pragma once

#include "esphome/components/switch/switch.h"
#include "../ld2410.h"

namespace esphome::ld2410 {

class BluetoothSwitch : public switch_::Switch, public Parented<LD2410Component> {
 public:
  BluetoothSwitch() = default;

 protected:
  void write_state(bool state) override;
};

}  // namespace esphome::ld2410

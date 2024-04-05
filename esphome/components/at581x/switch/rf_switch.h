#pragma once

#include "esphome/components/switch/switch.h"
#include "../at581x.h"

namespace esphome {
namespace at581x {

class RFSwitch : public switch_::Switch, public Parented<AT581XComponent> {
 protected:
  void write_state(bool state) override;
};

}  // namespace at581x
}  // namespace esphome

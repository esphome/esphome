#pragma once

#include "esphome/components/switch/switch.h"
#include "../ld2412.h"

namespace esphome {
namespace ld2412 {

class EngineeringModeSwitch : public switch_::Switch, public Parented<LD2412Component> {
 public:
  EngineeringModeSwitch() = default;

 protected:
  void write_state(bool state) override;
};

}  // namespace ld2412
}  // namespace esphome

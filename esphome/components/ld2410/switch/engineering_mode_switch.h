#pragma once

#include "esphome/components/switch/switch.h"
#include "../ld2410.h"

namespace esphome {
namespace ld2410 {

class EngineeringModeSwitch : public switch_::Switch, public Parented<LD2410Component> {
 public:
  EngineeringModeSwitch() = default;

 protected:
  void write_state(bool state) override;
};

}  // namespace ld2410
}  // namespace esphome

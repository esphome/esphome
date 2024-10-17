#pragma once

#include "esphome/components/switch/switch.h"
#include "../kt0803.h"

namespace esphome {
namespace kt0803 {

class AlcEnableSwitch : public switch_::Switch, public Parented<KT0803Component> {
 public:
  AlcEnableSwitch() = default;

 protected:
  void write_state(bool value) override;
};

}  // namespace kt0803
}  // namespace esphome

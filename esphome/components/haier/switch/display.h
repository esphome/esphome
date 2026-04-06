#pragma once

#include "esphome/components/switch/switch.h"
#include "../haier_base.h"

namespace esphome {
namespace haier {

class DisplaySwitch : public switch_::Switch, public Parented<HaierClimateBase> {
 public:
  DisplaySwitch() = default;

 protected:
  void write_state(bool state) override;
};

}  // namespace haier
}  // namespace esphome

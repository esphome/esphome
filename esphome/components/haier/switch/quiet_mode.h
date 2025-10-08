#pragma once

#include "esphome/components/switch/switch.h"
#include "../hon_climate.h"

namespace esphome {
namespace haier {

class QuietModeSwitch : public switch_::Switch, public Parented<HonClimate> {
 public:
  QuietModeSwitch() = default;

 protected:
  void write_state(bool state) override;
};

}  // namespace haier
}  // namespace esphome

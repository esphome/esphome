#pragma once

#include "esphome/components/switch/switch.h"
#include "../seeed_mr24hpc1.h"

namespace esphome {
namespace seeed_mr24hpc1 {

class UnderlyOpenFunctionSwitch : public switch_::Switch, public Parented<MR24HPC1Component> {
 public:
  UnderlyOpenFunctionSwitch() = default;

 protected:
  void write_state(bool state) override;
};

}  // namespace seeed_mr24hpc1
}  // namespace esphome

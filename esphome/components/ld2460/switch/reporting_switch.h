#pragma once

#include "esphome/components/switch/switch.h"
#include "../ld2460.h"

namespace esphome::ld2460 {

class ReportingSwitch : public switch_::Switch, public Parented<LD2460Component> {
 public:
  ReportingSwitch() = default;

 protected:
  void write_state(bool state) override;
};

}  // namespace esphome::ld2460

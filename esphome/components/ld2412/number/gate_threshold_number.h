#pragma once

#include "esphome/components/number/number.h"
#include "../ld2412.h"

namespace esphome::ld2412 {

class GateThresholdNumber : public number::Number, public Parented<LD2412Component> {
 public:
  GateThresholdNumber(uint8_t gate);

 protected:
  uint8_t gate_;
  void control(float value) override;
};

}  // namespace esphome::ld2412

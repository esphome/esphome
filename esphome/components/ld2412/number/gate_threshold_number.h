#pragma once

#include "esphome/components/number/number.h"
#include "../ld2412.h"

namespace esphome::ld2412 {

class GateThresholdNumber final : public number::Number, public Parented<LD2412Component> {
 public:
  explicit GateThresholdNumber(bool motion) : motion_(motion) {}

 protected:
  void control(float value) override;
  bool motion_;
};

}  // namespace esphome::ld2412

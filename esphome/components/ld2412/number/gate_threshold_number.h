#pragma once

#include "esphome/components/number/number.h"
#include "../ld2412.h"

namespace esphome::ld2412 {

class GateThresholdNumber final : public number::Number, public Parented<LD2412Component> {
 public:
  // Not "= default": that makes new(p) T() zero-fill the object at every codegen site before the ctor runs.
  GateThresholdNumber() {}

 protected:
  void control(float value) override;
};

}  // namespace esphome::ld2412

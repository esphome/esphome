#pragma once

#include "esphome/components/number/number.h"
#include "../ld2412.h"

namespace esphome::ld2412 {

class LightThresholdNumber final : public number::Number, public Parented<LD2412Component> {
 public:
  // User provided, not "= default": `new(p) LightThresholdNumber()` would zero-fill .bss that is already zero.
  LightThresholdNumber() {}

 protected:
  void control(float value) override;
};

}  // namespace esphome::ld2412

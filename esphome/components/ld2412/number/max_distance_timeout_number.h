#pragma once

#include "esphome/components/number/number.h"
#include "../ld2412.h"

namespace esphome::ld2412 {

class MaxDistanceTimeoutNumber : public number::Number, public Parented<LD2412Component> {
 public:
  MaxDistanceTimeoutNumber() = default;

 protected:
  void control(float value) override;
};

}  // namespace esphome::ld2412

#pragma once

#include "esphome/components/number/number.h"
#include "../seeed_mr24hpc1.h"

namespace esphome {
namespace seeed_mr24hpc1 {

class MotionThresholdNumber : public number::Number, public Parented<MR24HPC1Component> {
 public:
  MotionThresholdNumber() = default;

 protected:
  void control(float value) override;
};

}  // namespace seeed_mr24hpc1
}  // namespace esphome

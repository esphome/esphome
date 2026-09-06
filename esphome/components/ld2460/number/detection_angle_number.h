#pragma once

#include "esphome/components/number/number.h"
#include "../ld2460.h"

namespace esphome::ld2460 {

class DetectionAngleMinNumber : public number::Number, public Parented<LD2460Component> {
 public:
  DetectionAngleMinNumber() = default;

 protected:
  void control(float value) override;
};

class DetectionAngleMaxNumber : public number::Number, public Parented<LD2460Component> {
 public:
  DetectionAngleMaxNumber() = default;

 protected:
  void control(float value) override;
};

}  // namespace esphome::ld2460

#pragma once

#include "esphome/components/number/number.h"
#include "../si4713.h"

namespace esphome {
namespace si4713 {

class AcompThresholdNumber : public number::Number, public Parented<Si4713Component> {
 public:
  AcompThresholdNumber() = default;

 protected:
  void control(float value) override;
};

}  // namespace si4713
}  // namespace esphome
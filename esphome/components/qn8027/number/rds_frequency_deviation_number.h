#pragma once

#include "esphome/components/number/number.h"
#include "../qn8027.h"

namespace esphome {
namespace qn8027 {

class RDSFrequencyDeviationNumber : public number::Number, public Parented<QN8027Component> {
 public:
  RDSFrequencyDeviationNumber() = default;

 protected:
  void control(float value) override;
};

}  // namespace qn8027
}  // namespace esphome

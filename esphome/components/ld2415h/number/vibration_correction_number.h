#pragma once

#include "esphome/components/number/number.h"
#include "../ld2415h.h"

namespace esphome {
namespace ld2415h {

class VibrationCorrectionNumber : public number::Number, public Parented<LD2415HComponent> {
 public:
  VibrationCorrectionNumber() = default;

 protected:
  void control(float correction) override;
};

}  // namespace ld2415h
}  // namespace esphome

#pragma once

#include "esphome/components/number/number.h"
#include "../fyrtur_motor.h"

namespace esphome {
namespace fyrtur_motor {

class SetpointNumber : public number::Number, public Parented<FyrturMotorComponent> {
 public:
  SetpointNumber() = default;

 protected:
  void control(float value) override;
};

}  // namespace fyrtur_motor
}  // namespace esphome

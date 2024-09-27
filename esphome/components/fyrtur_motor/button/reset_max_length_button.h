#pragma once

#include "esphome/components/button/button.h"
#include "../fyrtur_motor.h"

namespace esphome {
namespace fyrtur_motor {

class ResetMaxLengthButton : public button::Button, public Parented<FyrturMotorComponent> {
 public:
  ResetMaxLengthButton() = default;

 protected:
  void press_action() override;
};

}  // namespace fyrtur_motor
}  // namespace esphome

#pragma once

#include "esphome/components/button/button.h"
#include "../fyrtur_motor.h"

namespace esphome {
namespace fyrtur_motor {

class StopButton : public button::Button, public Parented<FyrturMotorComponent> {
 public:
  StopButton() = default;

 protected:
  void press_action() override;
};

}  // namespace fyrtur_motor
}  // namespace esphome

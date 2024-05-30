#pragma once

#include "esphome/components/switch/switch.h"
#include "../fyrtur_motor.h"

namespace esphome {
namespace fyrtur_motor {

class OpenCloseSwitch : public switch_::Switch, public Parented<FyrturMotorComponent> {
 public:
  OpenCloseSwitch() = default;

 protected:
  void write_state(bool state) override;
};

}  // namespace fyrtur_motor
}  // namespace esphome

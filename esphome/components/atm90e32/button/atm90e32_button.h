#pragma once

#include "esphome/core/component.h"
#include "esphome/components/atm90e32/atm90e32.h"
#include "esphome/components/button/button.h"

namespace esphome {
namespace atm90e32 {

class ATM90E32CalibrationButton : public button::Button, public Parented<ATM90E32Component> {
 public:
  ATM90E32CalibrationButton() = default;

 protected:
  void press_action() override;
};

class ATM90E32ClearCalibrationButton : public button::Button, public Parented<ATM90E32Component> {
 public:
  ATM90E32ClearCalibrationButton() = default;

 protected:
  void press_action() override;
};

}  // namespace atm90e32
}  // namespace esphome

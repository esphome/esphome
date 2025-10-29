#pragma once

#include "esphome/core/component.h"
#include "esphome/components/atm90e32/atm90e32.h"
#include "esphome/components/button/button.h"

namespace esphome {
namespace atm90e32 {

class ATM90E32GainCalibrationButton : public button::Button, public Parented<ATM90E32Component> {
 public:
  ATM90E32GainCalibrationButton() = default;

 protected:
  void press_action() override;
};

class ATM90E32ClearGainCalibrationButton : public button::Button, public Parented<ATM90E32Component> {
 public:
  ATM90E32ClearGainCalibrationButton() = default;

 protected:
  void press_action() override;
};

class ATM90E32OffsetCalibrationButton : public button::Button, public Parented<ATM90E32Component> {
 public:
  ATM90E32OffsetCalibrationButton() = default;

 protected:
  void press_action() override;
};

class ATM90E32ClearOffsetCalibrationButton : public button::Button, public Parented<ATM90E32Component> {
 public:
  ATM90E32ClearOffsetCalibrationButton() = default;

 protected:
  void press_action() override;
};

class ATM90E32PowerOffsetCalibrationButton : public button::Button, public Parented<ATM90E32Component> {
 public:
  ATM90E32PowerOffsetCalibrationButton() = default;

 protected:
  void press_action() override;
};

class ATM90E32ClearPowerOffsetCalibrationButton : public button::Button, public Parented<ATM90E32Component> {
 public:
  ATM90E32ClearPowerOffsetCalibrationButton() = default;

 protected:
  void press_action() override;
};

}  // namespace atm90e32
}  // namespace esphome

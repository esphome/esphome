#pragma once

#include "esphome/core/component.h"
#include "esphome/components/atm90e32/atm90e32.h"
#include "esphome/components/button/button.h"

namespace esphome::atm90e32 {

class ATM90E32GainCalibrationButton final : public button::Button, public Parented<ATM90E32Component> {
 public:
  ATM90E32GainCalibrationButton() = default;

 protected:
  void press_action() override;
};

class ATM90E32ClearGainCalibrationButton final : public button::Button, public Parented<ATM90E32Component> {
 public:
  ATM90E32ClearGainCalibrationButton() = default;

 protected:
  void press_action() override;
};

class ATM90E32OffsetCalibrationButton final : public button::Button, public Parented<ATM90E32Component> {
 public:
  ATM90E32OffsetCalibrationButton() = default;

 protected:
  void press_action() override;
};

class ATM90E32ClearOffsetCalibrationButton final : public button::Button, public Parented<ATM90E32Component> {
 public:
  ATM90E32ClearOffsetCalibrationButton() = default;

 protected:
  void press_action() override;
};

class ATM90E32PowerOffsetCalibrationButton final : public button::Button, public Parented<ATM90E32Component> {
 public:
  ATM90E32PowerOffsetCalibrationButton() = default;

 protected:
  void press_action() override;
};

class ATM90E32ClearPowerOffsetCalibrationButton final : public button::Button, public Parented<ATM90E32Component> {
 public:
  ATM90E32ClearPowerOffsetCalibrationButton() = default;

 protected:
  void press_action() override;
};

}  // namespace esphome::atm90e32

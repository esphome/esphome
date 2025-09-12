#pragma once

#include "esphome/components/number/number.h"
#include "../dfrobot_c4001.h"

namespace esphome {
namespace dfrobot_c4001 {

class MinRangeNumber : public number::Number, public Parented<C4001Component> {
 protected:
  void control(float value) override;
};

class MaxRangeNumber : public number::Number, public Parented<C4001Component> {
 protected:
  void control(float value) override;
};

class TrigRangeNumber : public number::Number, public Parented<C4001Component> {
 protected:
  void control(float value) override;
};

class KeepSensitivityNumber : public number::Number, public Parented<C4001Component> {
 protected:
  void control(float value) override;
};

class TrigSensitivityNumber : public number::Number, public Parented<C4001Component> {
 protected:
  void control(float value) override;
};

class ConfirmDelayNumber : public number::Number, public Parented<C4001Component> {
 protected:
  void control(float value) override;
};

class DisappearDelayNumber : public number::Number, public Parented<C4001Component> {
 protected:
  void control(float value) override;
};

class ThresholdFactorNumber : public number::Number, public Parented<C4001Component> {
 protected:
  void control(float value) override;
};

}  // namespace dfrobot_c4001
}  // namespace esphome

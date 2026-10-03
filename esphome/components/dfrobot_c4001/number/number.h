#pragma once

#include "esphome/components/number/number.h"

#include "../dfrobot_c4001.h"

namespace esphome {
namespace dfrobot_c4001 {
class MaxRangeNumber : public number::Number, public Parented<DFRobotC4001Hub> {
 protected:
  void control(float value) override;
};
class MinRangeNumber : public number::Number, public Parented<DFRobotC4001Hub> {
 protected:
  void control(float value) override;
};
class TriggerRangeNumber : public number::Number, public Parented<DFRobotC4001Hub> {
 protected:
  void control(float value) override;
};
class HoldSensitivityNumber : public number::Number, public Parented<DFRobotC4001Hub> {
 protected:
  void control(float value) override;
};
class TriggerSensitivityNumber : public number::Number, public Parented<DFRobotC4001Hub> {
 protected:
  void control(float value) override;
};
class OnLatencyNumber : public number::Number, public Parented<DFRobotC4001Hub> {
 protected:
  void control(float value) override;
};
class OffLatencyNumber : public number::Number, public Parented<DFRobotC4001Hub> {
 protected:
  void control(float value) override;
};
class InhibitTimeNumber : public number::Number, public Parented<DFRobotC4001Hub> {
 protected:
  void control(float value) override;
};
class ThresholdFactorNumber : public number::Number, public Parented<DFRobotC4001Hub> {
 protected:
  void control(float value) override;
};
}  // namespace dfrobot_c4001
}  // namespace esphome

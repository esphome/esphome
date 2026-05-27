#pragma once

#include "../dfrobot_c4004.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace dfrobot_c4004 {

class C4004InstallHeightNumber : public number::Number, public Parented<C4004Component> {
 protected:
  void control(float value) override;
};

class C4004InstallZAngleNumber : public number::Number, public Parented<C4004Component> {
 protected:
  void control(float value) override;
};

class C4004RangeXMaxNumber : public number::Number, public Parented<C4004Component> {
 protected:
  void control(float value) override;
};

class C4004RangeXMinNumber : public number::Number, public Parented<C4004Component> {
 protected:
  void control(float value) override;
};

class C4004RangeYMaxNumber : public number::Number, public Parented<C4004Component> {
 protected:
  void control(float value) override;
};

class C4004RangeYMinNumber : public number::Number, public Parented<C4004Component> {
 protected:
  void control(float value) override;
};

class C4004TargetCountNumber : public number::Number, public Parented<C4004Component> {
 protected:
  void control(float value) override;
};

class C4004PeopleReportIntervalNumber : public number::Number, public Parented<C4004Component> {
 protected:
  void control(float value) override;
};

class C4004TrajectoryGenerateDistanceNumber : public number::Number, public Parented<C4004Component> {
 protected:
  void control(float value) override;
};

class C4004TrajectoryHoldTimeNumber : public number::Number, public Parented<C4004Component> {
 protected:
  void control(float value) override;
};

class C4004NoPersonDelayNumber : public number::Number, public Parented<C4004Component> {
 protected:
  void control(float value) override;
};

}  // namespace dfrobot_c4004
}  // namespace esphome

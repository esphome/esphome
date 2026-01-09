#pragma once

#include "esphome/components/number/number.h"
#include "../dfrobot_c4002.h"

namespace esphome {
namespace dfrobot_c4002 {

class MinDetectRangeNumber : public number::Number, public Parented<C4002Component> {
 protected:
  void control(float value) override;
};

class MaxDetectRangeNumber : public number::Number, public Parented<C4002Component> {
 protected:
  void control(float value) override;
};

class LightThresholdNumber : public number::Number, public Parented<C4002Component> {
 protected:
  void control(float value) override;
};

// ===== 区域 1 的最小 / 最大门编号 (0~11) =====
class Area1MinRangeNumber : public number::Number, public Parented<C4002Component> {
 protected:
  void control(float value) override;
};

class Area1MaxRangeNumber : public number::Number, public Parented<C4002Component> {
 protected:
  void control(float value) override;
};

// ===== 区域 2 =====
class Area2MinRangeNumber : public number::Number, public Parented<C4002Component> {
 protected:
  void control(float value) override;
};

class Area2MaxRangeNumber : public number::Number, public Parented<C4002Component> {
 protected:
  void control(float value) override;
};

// ===== 区域 3 =====
class Area3MinRangeNumber : public number::Number, public Parented<C4002Component> {
 protected:
  void control(float value) override;
};

class Area3MaxRangeNumber : public number::Number, public Parented<C4002Component> {
 protected:
  void control(float value) override;
};

class TargetDisappeardDelayTimeNumber : public number::Number, public Parented<C4002Component> {
 protected:
  void control(float value) override;
};

}  // namespace dfrobot_c4002
}  // namespace esphome

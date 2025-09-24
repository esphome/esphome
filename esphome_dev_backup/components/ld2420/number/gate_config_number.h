#pragma once

#include "esphome/components/number/number.h"
#include "../ld2420.h"

namespace esphome {
namespace ld2420 {

class LD2420TimeoutNumber : public number::Number, public Parented<LD2420Component> {
 public:
  LD2420TimeoutNumber() = default;

 protected:
  void control(float timeout) override;
};

class LD2420MinDistanceNumber : public number::Number, public Parented<LD2420Component> {
 public:
  LD2420MinDistanceNumber() = default;

 protected:
  void control(float min_gate) override;
};

class LD2420MaxDistanceNumber : public number::Number, public Parented<LD2420Component> {
 public:
  LD2420MaxDistanceNumber() = default;

 protected:
  void control(float max_gate) override;
};

class LD2420GateSelectNumber : public number::Number, public Parented<LD2420Component> {
 public:
  LD2420GateSelectNumber() = default;

 protected:
  void control(float gate_select) override;
};

class LD2420MoveSensFactorNumber : public number::Number, public Parented<LD2420Component> {
 public:
  LD2420MoveSensFactorNumber() = default;

 protected:
  void control(float move_factor) override;
};

class LD2420StillSensFactorNumber : public number::Number, public Parented<LD2420Component> {
 public:
  LD2420StillSensFactorNumber() = default;

 protected:
  void control(float still_factor) override;
};

class LD2420StillThresholdNumbers : public number::Number, public Parented<LD2420Component> {
 public:
  LD2420StillThresholdNumbers() = default;
  LD2420StillThresholdNumbers(uint8_t gate);

 protected:
  uint8_t gate_;
  void control(float still_threshold) override;
};

class LD2420MoveThresholdNumbers : public number::Number, public Parented<LD2420Component> {
 public:
  LD2420MoveThresholdNumbers() = default;
  LD2420MoveThresholdNumbers(uint8_t gate);

 protected:
  uint8_t gate_;
  void control(float move_threshold) override;
};

}  // namespace ld2420
}  // namespace esphome

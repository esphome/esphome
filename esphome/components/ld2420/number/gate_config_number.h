#pragma once

#include "esphome/components/number/number.h"
#include "../ld2420.h"

namespace esphome {
namespace ld2420 {

class LD2420TimeoutNumber : public number::Number, public Parented<LD2420Component> {
 public:
  LD2420TimeoutNumber() = default;

 protected:
  void control(float value) override;
};

class LD2420MinDistanceNumber : public number::Number, public Parented<LD2420Component> {
 public:
  LD2420MinDistanceNumber() = default;

 protected:
  void control(float value) override;
};

class LD2420MaxDistanceNumber : public number::Number, public Parented<LD2420Component> {
 public:
  LD2420MaxDistanceNumber() = default;

 protected:
  void control(float value) override;
};

class LD2420GateSelectNumber : public number::Number, public Parented<LD2420Component> {
 public:
  LD2420GateSelectNumber() = default;

 protected:
  void control(float value) override;
};

class LD2420StillThresholdNumber : public number::Number, public Parented<LD2420Component> {
 public:
  LD2420StillThresholdNumber() = default;

 protected:
  void control(float value) override;
};

class LD2420MoveThresholdNumber : public number::Number, public Parented<LD2420Component> {
 public:
  LD2420MoveThresholdNumber() = default;

 protected:
  void control(float value) override;
};

}  // namespace ld2420
}  // namespace esphome

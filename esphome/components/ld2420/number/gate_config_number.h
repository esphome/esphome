#pragma once

#include "esphome/components/number/number.h"
#include "../ld2420.h"

namespace esphome::ld2420 {

class LD2420MinDistanceNumber final : public number::Number, public Parented<LD2420Component> {
 public:
  LD2420MinDistanceNumber() = default;

 protected:
  void control(float min_gate) override;
};

class LD2420MaxDistanceNumber final : public number::Number, public Parented<LD2420Component> {
 public:
  LD2420MaxDistanceNumber() = default;

 protected:
  void control(float max_gate) override;
};

class LD2420GateSelectNumber final : public number::Number, public Parented<LD2420Component> {
 public:
  LD2420GateSelectNumber() = default;

 protected:
  void control(float gate_select) override;
};

}  // namespace esphome::ld2420

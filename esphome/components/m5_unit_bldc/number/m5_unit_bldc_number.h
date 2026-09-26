#pragma once

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"
#include "esphome/components/m5_unit_bldc/m5_unit_bldc.h"

namespace esphome::m5_unit_bldc {

enum class NumberType : uint8_t { PWM, TARGET_RPM };

/// A `number` entity driving either the open-loop PWM duty or the closed-loop target RPM of an
/// M5Unit-BLDC. Which register it writes to is fixed at construction time by `type`.
class M5UnitBldcNumber : public number::Number, public Parented<M5UnitBldc> {
 public:
  explicit M5UnitBldcNumber(NumberType type) : type_(type) {}

 protected:
  void control(float value) override;

  NumberType type_;
};

}  // namespace esphome::m5_unit_bldc

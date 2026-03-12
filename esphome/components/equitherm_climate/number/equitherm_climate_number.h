#pragma once

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace equitherm_climate {

class EquithermClimate;

// Number types
enum EquithermClimateNumberType {
  EQUITHERM_NUMBER_SLOPE = 0,
  EQUITHERM_NUMBER_EXPONENT = 1,
  EQUITHERM_NUMBER_SHIFT = 2,
  EQUITHERM_NUMBER_TARGET_DIFF_FACTOR = 3,
  EQUITHERM_NUMBER_KP = 4,
  EQUITHERM_NUMBER_KI = 5,
  EQUITHERM_NUMBER_KD = 6,
};

class EquithermClimateNumber : public number::Number, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_parent(EquithermClimate *parent) { parent_ = parent; }
  void set_type(EquithermClimateNumberType type) { type_ = type; }

 protected:
  void control(float value) override;

  EquithermClimate *parent_;
  EquithermClimateNumberType type_;
};

}  // namespace equitherm_climate
}  // namespace esphome

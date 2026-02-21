#pragma once

#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"

namespace esphome::number {

enum NumberMode : uint8_t {
  NUMBER_MODE_AUTO = 0,
  NUMBER_MODE_BOX = 1,
  NUMBER_MODE_SLIDER = 2,
};

class NumberTraits {
 public:
  // Set/get the number value boundaries.
  void set_min_value(float min_value) { min_value_ = min_value; }
  float get_min_value() const { return min_value_; }
  void set_max_value(float max_value) { max_value_ = max_value; }
  float get_max_value() const { return max_value_; }

  // Set/get the step size for incrementing or decrementing the number value.
  void set_step(float step) { step_ = step; }
  float get_step() const { return step_; }

  // Set/get the frontend mode.
  void set_mode(NumberMode mode) { this->mode_ = mode; }
  NumberMode get_mode() const { return this->mode_; }

  // Deprecated: use Number::get_device_class_ref() instead.
  // Delegates to parent Number's EntityBase via offsetof.
  ESPDEPRECATED("Use number->get_device_class_ref() instead. Removed in 2027.2.0", "2026.8.0")
  StringRef get_device_class_ref() const;

  // Deprecated: use Number::get_unit_of_measurement_ref() instead.
  // Delegates to parent Number's EntityBase via offsetof.
  ESPDEPRECATED("Use number->get_unit_of_measurement_ref() instead. Removed in 2027.2.0", "2026.8.0")
  StringRef get_unit_of_measurement_ref() const;

 protected:
  float min_value_ = NAN;
  float max_value_ = NAN;
  float step_ = NAN;
  NumberMode mode_{NUMBER_MODE_AUTO};
};

}  // namespace esphome::number

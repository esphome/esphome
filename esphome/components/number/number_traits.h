#pragma once

#include "esphome/core/helpers.h"

namespace esphome {
namespace number {

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

  /// Manually set the unit of measurement.
  void set_unit_of_measurement(const std::string &unit_of_measurement);
  /// Get the unit of measurement, using the manual override if set.
  std::string get_unit_of_measurement();

  // Set/get the frontend mode.
  void set_mode(NumberMode mode) { this->mode_ = mode; }
  NumberMode get_mode() const { return this->mode_; }

  // Set/get the device class.
  void set_device_class(const std::string &device_class);
  std::string get_device_class();

 protected:
  float min_value_ = NAN;
  float max_value_ = NAN;
  float step_ = NAN;
  optional<std::string> unit_of_measurement_;  ///< Unit of measurement override
  NumberMode mode_{NUMBER_MODE_AUTO};
  optional<std::string> device_class_;
};

}  // namespace number
}  // namespace esphome

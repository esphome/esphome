#pragma once

#include <cmath>
#include <cstdint>
#include <type_traits>

namespace esphome::number {

enum NumberMode : uint8_t {
  NUMBER_MODE_AUTO = 0,
  NUMBER_MODE_BOX = 1,
  NUMBER_MODE_SLIDER = 2,
};

/// Boundaries and step for a number; codegen shares one PROGMEM table between numbers with the same values.
struct NumberRange {
  float min_value;
  float max_value;
  float step;
};
// Read straight from flash on ESP8266, so every field must stay a word.
static_assert(std::is_same_v<decltype(NumberRange::min_value), float> &&
                  std::is_same_v<decltype(NumberRange::max_value), float> &&
                  std::is_same_v<decltype(NumberRange::step), float>,
              "NumberRange fields must stay float");

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

 protected:
  float min_value_ = NAN;
  float max_value_ = NAN;
  float step_ = NAN;
  NumberMode mode_{NUMBER_MODE_AUTO};  // Keep in sync with DEFAULT_MODE in __init__.py
};

}  // namespace esphome::number

#pragma once

#include <cmath>
#include <cstdint>

#include "esphome/core/hal.h"

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
// Read straight from flash with aligned word loads on ESP8266, so every field must stay a word.
static_assert(sizeof(NumberRange) == 3 * sizeof(float), "NumberRange fields must stay 32-bit words");

/// Range of a number that never got one; NAN like an unset bound.
inline constexpr NumberRange DEFAULT_NUMBER_RANGE PROGMEM = {NAN, NAN, NAN};

class Number;

class NumberTraits {
 public:
  // Set/get the number value boundaries.
  void set_min_value(float min_value) { this->writable_range_()->min_value = min_value; }
  float get_min_value() const { return this->range_->min_value; }
  void set_max_value(float max_value) { this->writable_range_()->max_value = max_value; }
  float get_max_value() const { return this->range_->max_value; }

  // Set/get the step size for incrementing or decrementing the number value.
  void set_step(float step) { this->writable_range_()->step = step; }
  float get_step() const { return this->range_->step; }

  /// Use a constant range, usually a PROGMEM table shared by every number with the same values.
  void set_range(const NumberRange *range);
  /// The range in use, to share it with another number (it may be in PROGMEM).
  const NumberRange *get_range() const { return this->range_; }
  /// Writable RAM for bounds that change after setup; the setters then write there.
  void set_range_storage(NumberRange *storage) {
    this->range_ = storage;
    this->owns_range_ = true;
  }

  // Set/get the frontend mode.
  void set_mode(NumberMode mode) { this->mode_ = mode; }
  NumberMode get_mode() const { return this->mode_; }

 protected:
  friend class Number;

  // Without storage, the first set_min_value/set_max_value/set_step copies the range into a heap block of its own.
  NumberRange *writable_range_();

  const NumberRange *range_{&DEFAULT_NUMBER_RANGE};
  NumberMode mode_{NUMBER_MODE_AUTO};  // Keep in sync with DEFAULT_MODE in __init__.py
  bool owns_range_{false};
};

}  // namespace esphome::number

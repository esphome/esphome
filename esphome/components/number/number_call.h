#pragma once

#include "esphome/core/helpers.h"
#include "number_traits.h"

namespace esphome {
namespace number {

class Number;

class NumberCall {
 public:
  explicit NumberCall(Number *parent) : parent_(parent) {}
  void perform();

  NumberCall &set_value(float value);
  const optional<float> &get_value() const { return value_; }

 protected:
  Number *const parent_;
  optional<float> value_;
};

}  // namespace number
}  // namespace esphome

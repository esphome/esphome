#pragma once

#include "esphome/core/helpers.h"
#include "number_traits.h"

namespace esphome {
namespace number {

class Number;

enum NumberOperation {
  NUMBER_OP_NONE,
  NUMBER_OP_SET,
  NUMBER_OP_INCREMENT,
  NUMBER_OP_DECREMENT,
  NUMBER_OP_TO_MIN,
  NUMBER_OP_TO_MAX,
};

class NumberCall {
 public:
  explicit NumberCall(Number *parent) : parent_(parent) {}
  void perform();

  NumberCall &set_value(float value);
  NumberCall &number_increment(bool cycle);
  NumberCall &number_decrement(bool cycle);
  NumberCall &number_to_min();
  NumberCall &number_to_max();

  NumberCall &with_operation(NumberOperation operation);
  NumberCall &with_value(float value);
  NumberCall &with_cycle(bool cycle);

 protected:
  Number *const parent_;
  NumberOperation operation_{NUMBER_OP_NONE};
  optional<float> value_;
  bool cycle_;
};

}  // namespace number
}  // namespace esphome

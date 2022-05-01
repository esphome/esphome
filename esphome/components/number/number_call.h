#pragma once

#include "esphome/core/helpers.h"
#include "number_traits.h"

namespace esphome {
namespace number {

class Number;

enum NumberOperation {
  NUMBER_OP_NONE,
  NUMBER_OP_SET,
  NUMBER_OP_NEXT,
  NUMBER_OP_PREVIOUS,
  NUMBER_OP_FIRST,
  NUMBER_OP_LAST,
};

class NumberCall {
 public:
  explicit NumberCall(Number *parent) : parent_(parent) {}
  void perform();

  NumberCall &set_value(float value);
  const optional<float> &get_value() const { return value_; }

  NumberCall &number_next(bool cycle);
  NumberCall &number_previous(bool cycle);
  NumberCall &number_first();
  NumberCall &number_last();

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

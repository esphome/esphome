#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "number_traits.h"

namespace esphome::number {

class Number;

enum NumberOperation : uint8_t {
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
  void log_perform_warning_(const LogString *message);
  void log_perform_warning_value_range_(const LogString *comparison, const LogString *limit_type, float val,
                                        float limit);

  Number *const parent_;
  optional<float> value_;
  NumberOperation operation_{NUMBER_OP_NONE};
  bool cycle_{false};
};

}  // namespace esphome::number

#include "number_call.h"
#include "number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace number {

static const char *const TAG = "number";

NumberCall &NumberCall::set_value(float value) {
  return this->with_operation(NUMBER_OP_SET).with_value(value);
}

NumberCall &NumberCall::number_next(bool cycle) {
  return this->with_operation(NUMBER_OP_NEXT).with_cycle(cycle);
}

NumberCall &NumberCall::number_previous(bool cycle) {
  return this->with_operation(NUMBER_OP_PREVIOUS).with_cycle(cycle);
}

NumberCall &NumberCall::number_first() {
  return this->with_operation(NUMBER_OP_FIRST);
}

NumberCall &NumberCall::number_last() {
  return this->with_operation(NUMBER_OP_LAST);
}

NumberCall &NumberCall::with_operation(NumberOperation operation) {
  this->operation_ = operation;
  return *this;
}

NumberCall &NumberCall::with_value(float value) {
  this->value_ = value;
  return *this;
}

NumberCall &NumberCall::with_cycle(bool cycle) {
  this->cycle_ = cycle;
  return *this;
}

void NumberCall::perform() {
  auto *parent = this->parent_;
  const auto *name = parent->get_name().c_str();
  const auto &traits = parent->traits;

  if (this->operation_ == NUMBER_OP_NONE) {
    ESP_LOGW(TAG, "'%s' - NumberCall performed without selecting an operation", name);
    return;
  }

  float target_value = NAN;
  float min_value = traits.get_min_value();
  float max_value = traits.get_max_value();

  if (this->operation_ == NUMBER_OP_SET) {
    ESP_LOGD(TAG, "'%s' - Setting number value", name);
    if (!this->value_.has_value() || std::isnan(*this->value_)) {
      ESP_LOGW(TAG, "'%s' - No value set for NumberCall", name);
      return;
    }
    target_value = this->value_.value();
  } else if (this->operation_ == NUMBER_OP_FIRST) {
    target_value = std::isnan(min_value) ? std::numeric_limits<float>::min() : min_value;
  } else if (this->operation_ == NUMBER_OP_LAST) {
    target_value = std::isnan(max_value) ? std::numeric_limits<float>::max() : max_value;
  } else if (this->operation_ == NUMBER_OP_NEXT) {
    ESP_LOGD(TAG, "'%s' - Next number, with%s cycling", name, this->cycle_ ? "" : "out");
    if (!parent->has_state()) {
      ESP_LOGW(TAG, "'%s' - Can't set next number through NumberCall: no active state to modify", name);
      return;
    } else {
      target_value = parent->state + traits.get_step();
      if (target_value > max_value) {
        if (this->cycle_) {
          target_value = std::isnan(min_value) ? std::numeric_limits<float>::min() : min_value;
        } else {
          target_value = max_value;
        }
      }
    } 
  } else if (this->operation_ == NUMBER_OP_PREVIOUS) {
    ESP_LOGD(TAG, "'%s' - Previous number, with%s cycling", name, this->cycle_ ? "" : "out");
    if (!parent->has_state()) {
      ESP_LOGW(TAG, "'%s' - Can't set previous number through NumberCall: no active state to modify", name);
      return;
    } else {
      target_value = parent->state - traits.get_step();
      if (target_value < min_value) {
        if (this->cycle_) {
          target_value = std::isnan(max_value) ? std::numeric_limits<float>::max() : max_value;
        } else {
          target_value = min_value;
        }
      }
    }
  }

  if (target_value < min_value) {
    ESP_LOGW(TAG, "'%s' - Value %f must not be less than minimum %f", name, target_value, min_value);
    return;
  }
  if (target_value > max_value) {
    ESP_LOGW(TAG, "'%s' - Value %f must not be greater than maximum %f", name, target_value, max_value);
    return;
  }

  ESP_LOGD(TAG, "  New number value: %f", target_value);
  this->parent_->control(target_value);
}

}  // namespace number
}  // namespace esphome

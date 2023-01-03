#include "number_call.h"
#include "number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace number {

static const char *const TAG = "number";

NumberCall &NumberCall::set_value(float value) { return this->with_operation(NUMBER_OP_SET).with_value(value); }

NumberCall &NumberCall::number_increment(bool cycle) {
  return this->with_operation(NUMBER_OP_INCREMENT).with_cycle(cycle);
}

NumberCall &NumberCall::number_decrement(bool cycle) {
  return this->with_operation(NUMBER_OP_DECREMENT).with_cycle(cycle);
}

NumberCall &NumberCall::number_to_min() { return this->with_operation(NUMBER_OP_TO_MIN); }

NumberCall &NumberCall::number_to_max() { return this->with_operation(NUMBER_OP_TO_MAX); }

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
  } else if (this->operation_ == NUMBER_OP_TO_MIN) {
    if (std::isnan(min_value)) {
      ESP_LOGW(TAG, "'%s' - Can't set to min value through NumberCall: no min_value defined", name);
    } else {
      target_value = min_value;
    }
  } else if (this->operation_ == NUMBER_OP_TO_MAX) {
    if (std::isnan(max_value)) {
      ESP_LOGW(TAG, "'%s' - Can't set to max value through NumberCall: no max_value defined", name);
    } else {
      target_value = max_value;
    }
  } else if (this->operation_ == NUMBER_OP_INCREMENT) {
    ESP_LOGD(TAG, "'%s' - Increment number, with%s cycling", name, this->cycle_ ? "" : "out");
    if (!parent->has_state()) {
      ESP_LOGW(TAG, "'%s' - Can't increment number through NumberCall: no active state to modify", name);
      return;
    }
    auto step = traits.get_step();
    target_value = parent->state + (std::isnan(step) ? 1 : step);
    if (target_value > max_value) {
      if (this->cycle_ && !std::isnan(min_value)) {
        target_value = min_value;
      } else {
        target_value = max_value;
      }
    }
  } else if (this->operation_ == NUMBER_OP_DECREMENT) {
    ESP_LOGD(TAG, "'%s' - Decrement number, with%s cycling", name, this->cycle_ ? "" : "out");
    if (!parent->has_state()) {
      ESP_LOGW(TAG, "'%s' - Can't decrement number through NumberCall: no active state to modify", name);
      return;
    }
    auto step = traits.get_step();
    target_value = parent->state - (std::isnan(step) ? 1 : step);
    if (target_value < min_value) {
      if (this->cycle_ && !std::isnan(max_value)) {
        target_value = max_value;
      } else {
        target_value = min_value;
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

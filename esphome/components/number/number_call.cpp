#include "number_call.h"
#include "number.h"
#include "esphome/core/log.h"

namespace esphome::number {

static const char *const TAG = "number";

// Helper functions to reduce code size for logging
void NumberCall::log_perform_warning_(const LogString *message) {
  ESP_LOGW(TAG, "'%s': %s", this->parent_->get_name().c_str(), LOG_STR_ARG(message));
}

void NumberCall::log_perform_warning_value_range_(const LogString *comparison, const LogString *limit_type, float val,
                                                  float limit) {
  ESP_LOGW(TAG, "'%s': %f %s %s %f", this->parent_->get_name().c_str(), val, LOG_STR_ARG(comparison),
           LOG_STR_ARG(limit_type), limit);
}

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
    this->log_perform_warning_(LOG_STR("No operation"));
    return;
  }

  float target_value = NAN;
  float min_value = traits.get_min_value();
  float max_value = traits.get_max_value();

  if (this->operation_ == NUMBER_OP_SET) {
    ESP_LOGD(TAG, "'%s': Setting value", name);
    if (!this->value_.has_value() || std::isnan(*this->value_)) {
      this->log_perform_warning_(LOG_STR("No value"));
      return;
    }
    target_value = this->value_.value();
  } else if (this->operation_ == NUMBER_OP_TO_MIN) {
    if (std::isnan(min_value)) {
      this->log_perform_warning_(LOG_STR("min undefined"));
    } else {
      target_value = min_value;
    }
  } else if (this->operation_ == NUMBER_OP_TO_MAX) {
    if (std::isnan(max_value)) {
      this->log_perform_warning_(LOG_STR("max undefined"));
    } else {
      target_value = max_value;
    }
  } else if (this->operation_ == NUMBER_OP_INCREMENT) {
    ESP_LOGD(TAG, "'%s': Increment with%s cycling", name, this->cycle_ ? "" : "out");
    if (!parent->has_state()) {
      this->log_perform_warning_(LOG_STR("Can't increment, no state"));
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
    ESP_LOGD(TAG, "'%s': Decrement with%s cycling", name, this->cycle_ ? "" : "out");
    if (!parent->has_state()) {
      this->log_perform_warning_(LOG_STR("Can't decrement, no state"));
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
    this->log_perform_warning_value_range_(LOG_STR("<"), LOG_STR("min"), target_value, min_value);
    return;
  }
  if (target_value > max_value) {
    this->log_perform_warning_value_range_(LOG_STR(">"), LOG_STR("max"), target_value, max_value);
    return;
  }

  ESP_LOGD(TAG, "  New value: %f", target_value);
  this->parent_->control(target_value);
}

}  // namespace esphome::number

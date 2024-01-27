#include "input_datetime_call.h"
#include "esphome/core/log.h"
#include "input_datetime.h"

namespace esphome {
namespace input_datetime {

static const char *const TAG = "input_datetime";

InputDatetimeCall &InputDatetimeCall::set_value(const std::string value) {
  return this->with_operation(INPUT_DATETIME_OP_SET_VALUE).with_value(value);
}

InputDatetimeCall &InputDatetimeCall::set_has_date(const bool has_date) {
  return this->with_operation(INPUT_DATETIME_OP_SET_HAS_DATE).with_has_date(has_date);
}

InputDatetimeCall &InputDatetimeCall::set_has_time(const bool has_time) {
  return this->with_operation(INPUT_DATETIME_OP_SET_HAS_TIME).with_has_time(has_time);
}

InputDatetimeCall &InputDatetimeCall::with_value(std::string value) {
  ESPTime time{0};
  ESP_LOGD("mydebug", value.c_str());
  ESP_LOGD("mydebug", "HAS_DATETIME_STRING_TIME_ONLY: %d", HAS_DATETIME_STRING_TIME_ONLY(value));
  ESP_LOGD("mydebug", "HAS_DATETIME_STRING_DATE_ONLY: %d", HAS_DATETIME_STRING_DATE_ONLY(value));
  ESP_LOGD("mydebug", "HAS_DATETIME_STRING_DATE_AND_TIME: %d", HAS_DATETIME_STRING_DATE_AND_TIME(value));
  if (!parent_->has_date && !parent_->has_time) {
    ESP_LOGW(TAG, "Trying to set datetime value, but neither 'has_date' nor 'has_time' was set");
  } else if (parent_->has_date && parent_->has_time &&
             (HAS_DATETIME_STRING_DATE_ONLY(value) ||
              HAS_DATETIME_STRING_TIME_ONLY(value) && !HAS_DATETIME_STRING_DATE_AND_TIME(value))) {
    ESP_LOGW(TAG, "Time string was provided in the wrong format! Expected date and time.");
  } else if (parent_->has_date && !parent_->has_time &&
             (HAS_DATETIME_STRING_TIME_ONLY(value) ||
              HAS_DATETIME_STRING_DATE_AND_TIME(value) && !HAS_DATETIME_STRING_DATE_ONLY(value))) {
    ESP_LOGW(TAG, "Time string was provided in the wrong format! Expected date only.");
  } else if (!parent_->has_date && parent_->has_time &&
             (!HAS_DATETIME_STRING_DATE_ONLY(value) || HAS_DATETIME_STRING_DATE_AND_TIME(value)) &&
             !HAS_DATETIME_STRING_TIME_ONLY(value)) {
    ESP_LOGW(TAG, "Time string was provided in the wrong format! Expected time only.");
  } else {
    this->value_ = value;
  }
  return *this;
}

InputDatetimeCall &InputDatetimeCall::with_has_date(bool has_date) {
  this->has_date_ = has_date;
  return *this;
}

InputDatetimeCall &InputDatetimeCall::with_has_time(bool has_time) {
  this->has_time_ = has_time;
  return *this;
}

InputDatetimeCall &InputDatetimeCall::with_operation(InputDatetimeOperation operation) {
  this->operation_ = operation;
  return *this;
}

void InputDatetimeCall::perform() {
  auto *parent = this->parent_;
  const auto *name = parent->get_name().c_str();
  const auto &traits = parent->traits;

  std::string target_value{0};

  // load stored values if there are any
  if (!this->value_.has_value()) {
    ESP_LOGW(TAG, "'%s' - InputDatetime performed without setting a value", name);
    return;
  }

  switch (this->operation_) {
    case INPUT_DATETIME_OP_NONE:
      ESP_LOGW(TAG, "'%s' - InputDatetime performed without selecting an operation", name);
      return;
    case INPUT_DATETIME_OP_SET_VALUE:
      target_value = this->value_.value();
      std::string format = this->parent_->has_date && this->parent_->has_time ? "%F %T"
                           : this->parent_->has_date                          ? "%F"
                           : this->parent_->has_time                          ? "%T"
                                                                              : "";
      ESP_LOGD(TAG, "'%s' - Setting InputDatetime value: %s", name, target_value);
      break;
  }

  this->parent_->control(target_value);
}

}  // namespace input_datetime
}  // namespace esphome

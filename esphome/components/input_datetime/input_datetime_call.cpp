#include "input_datetime_call.h"
#include "esphome/core/log.h"
#include "input_datetime.h"

namespace esphome {
namespace input_datetime {

static const char *const TAG = "input_datetime";

InputDatetimeCall &InputDatetimeCall::set_value(const std::string value) {
  return this->with_operation(INPUT_DATETIME_OP_SET_VALUE).with_value(value);
}

InputDatetimeCall &InputDatetimeCall::with_value(std::string value) {
  ESPTime time{0};
  ESP_LOGD("mydebug", value.c_str());
  ESP_LOGD("mydebug", "HAS_DATETIME_STRING_TIME_ONLY: %d", HAS_DATETIME_STRING_TIME_ONLY(value));
  ESP_LOGD("mydebug", "HAS_DATETIME_STRING_DATE_ONLY: %d", HAS_DATETIME_STRING_DATE_ONLY(value));
  ESP_LOGD("mydebug", "HAS_DATETIME_STRING_DATE_AND_TIME: %d", HAS_DATETIME_STRING_DATE_AND_TIME(value));

  if (!validate_datetime_string(value) || !ESPTime::strptime(value, time)) {
    ESP_LOGW(TAG, "'%s' - InputDatetime tried to set an invalid Datetime string", this->parent_->get_name().c_str());
    return *this;
  }

  this->value_.value() = value;
  return *this;
}

InputDatetimeCall &InputDatetimeCall::with_operation(InputDatetimeOperation operation) {
  this->operation_ = operation;
  return *this;
}

void InputDatetimeCall::perform() {
  auto *parent = this->parent_;
  const auto *name = parent->get_name().c_str();

  std::string target_value{};

  switch (this->operation_) {
    case INPUT_DATETIME_OP_NONE:
      ESP_LOGW(TAG, "'%s' - InputDatetime performed without selecting an operation", name);
      return;
    case INPUT_DATETIME_OP_SET_VALUE:
      if (!this->value_.has_value()) {
        ESP_LOGW(TAG, "'%s' - InputDatetime performed without setting a value", name);
        return;
      }
      if (!validate_datetime_string(this->value_.value())) {
        ESP_LOGW(TAG, "'%s' - InputDatetime performed without a valid datetime value", name);
        return;
      }

      target_value = this->value_.value();
      ESP_LOGD(TAG, "'%s' - Setting InputDatetime value: %s", name, target_value);
      break;
  }

  this->parent_->control(target_value);
}

bool InputDatetimeCall::validate_datetime_string(std::string value) {
  if (HAS_DATETIME_STRING_TIME_ONLY(value))
    return true;
  else if (HAS_DATETIME_STRING_DATE_ONLY(value))
    return true;
  else if (HAS_DATETIME_STRING_DATE_AND_TIME(value))
    return true;
  return false;
}

}  // namespace input_datetime
}  // namespace esphome

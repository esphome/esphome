#include "datetime_call.h"
#include "esphome/core/log.h"
#include "datetime.h"

namespace esphome {
namespace datetime {

static const char *const TAG = "datetime";

DatetimeCall &DatetimeCall::set_value(const std::string &value) {
  return this->with_operation(DATETIME_OP_SET_VALUE).with_value(value);
}

DatetimeCall &DatetimeCall::with_value(const std::string &value) {
  this->value_ = value;
  return *this;
}

DatetimeCall &DatetimeCall::with_operation(DatetimeOperation operation) {
  this->operation_ = operation;
  return *this;
}

void DatetimeCall::perform() {
  auto *parent = this->parent_;
  const auto *name = parent->get_name().c_str();

  std::string target_value{};

  switch (this->operation_) {
    case DATETIME_OP_NONE:
      ESP_LOGW(TAG, "'%s' - Datetime performed without selecting an operation", name);
      return;
    case DATETIME_OP_SET_VALUE:
      if (!this->value_.has_value()) {
        ESP_LOGW(TAG, "'%s' - Datetime performed without setting a value", name);
        return;
      }
      if (!validate_datetime_string_(this->value_.value())) {
        ESP_LOGW(TAG, "'%s' - Datetime performed without a valid datetime value, value was '%s'", name,
                 this->value_.value().c_str());
        return;
      }

      target_value = this->value_.value();
      ESP_LOGD(TAG, "'%s' - Setting Datetime value: %s", name, target_value.c_str());
      break;
  }

  this->parent_->control(target_value);
}

bool DatetimeCall::validate_datetime_string_(const std::string &value) {
  return HAS_DATETIME_STRING_DATE_OR_TIME(value);
}

}  // namespace datetime
}  // namespace esphome

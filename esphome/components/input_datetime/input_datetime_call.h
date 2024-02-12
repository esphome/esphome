#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/time.h"
#include <regex>

namespace esphome {
namespace datetime {

#define HAS_DATETIME_STRING_DATE_ONLY(value) std::regex_match(value, std::regex(R"(^\d{4}-\d{2}-\d{2}$)"))
#define HAS_DATETIME_STRING_TIME_ONLY(value) std::regex_match(value, std::regex(R"(^\d{2}:\d{2}(:\d{2})?$)"))
#define HAS_DATETIME_STRING_DATE_AND_TIME(value) \
  std::regex_match(value, std::regex(R"(^\d{4}-\d{2}-\d{2}[ T]\d{2}:\d{2}(:\d{2})?$)"))
#define HAS_DATETIME_STRING_DATE_OR_TIME(value) \
  std::regex_match(value, std::regex(R"(^\d{4}-\d{2}-\d{2}([ T]\d{2}:\d{2}(:\d{2})?)?|\d{2}:\d{2}(:\d{2})?$)"))

class Datetime;

enum DatetimeOperation {
  DATETIME_OP_NONE,
  DATETIME_OP_SET_VALUE,
  DATETIME_OP_SET_HAS_DATE,
  DATETIME_OP_SET_HAS_TIME,
};

class DatetimeCall {
 public:
  explicit DatetimeCall(Datetime *parent) : parent_(parent) {}
  void perform();
  DatetimeCall &set_value(const std::string value);
  DatetimeCall &set_has_date(const bool has_date);
  DatetimeCall &set_has_time(const bool has_time);

  DatetimeCall &with_operation(DatetimeOperation operation);
  DatetimeCall &with_value(std::string value);
  DatetimeCall &with_has_date(bool has_date);
  DatetimeCall &with_has_time(bool has_time);

 protected:
  Datetime *const parent_;
  DatetimeOperation operation_{DATETIME_OP_NONE};
  optional<std::string> value_;
  optional<bool> has_date_;
  optional<bool> has_time_;
};

}  // namespace datetime
}  // namespace esphome

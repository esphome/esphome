#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/time.h"
#include <regex>

namespace esphome {
namespace input_datetime {

#define HAS_DATETIME_STRING_DATE_ONLY(value) std::regex_match(value, std::regex(R"(^\d{4}-\d{2}-\d{2}$)"))
#define HAS_DATETIME_STRING_TIME_ONLY(value) std::regex_match(value, std::regex(R"(^\d{2}:\d{2}(:\d{2})?$)"))
#define HAS_DATETIME_STRING_DATE_AND_TIME(value) \
  std::regex_match(value, std::regex(R"(^\d{4}-\d{2}-\d{2}[ T]\d{2}:\d{2}(:\d{2})?$)"))
#define HAS_DATETIME_STRING_DATE_OR_TIME(value) \
  std::regex_match(value, std::regex(R"(^\d{4}-\d{2}-\d{2}([ T]\d{2}:\d{2}(:\d{2})?)?|\d{2}:\d{2}(:\d{2})?$)"))

struct ESPDatetime {
  ESPTime time;
  bool has_date;
  bool has_time;
};

class InputDatetime;

enum InputDatetimeOperation {
  INPUT_DATETIME_OP_NONE,
  INPUT_DATETIME_OP_SET_VALUE,
  INPUT_DATETIME_OP_SET_HAS_DATE,
  INPUT_DATETIME_OP_SET_HAS_TIME,
};

class InputDatetimeCall {
 public:
  explicit InputDatetimeCall(InputDatetime *parent) : parent_(parent) {}
  void perform();
  InputDatetimeCall &set_value(const std::string value);

  InputDatetimeCall &with_operation(InputDatetimeOperation operation);
  InputDatetimeCall &with_value(std::string value);

 protected:
  InputDatetime *const parent_;
  InputDatetimeOperation operation_{INPUT_DATETIME_OP_NONE};
  optional<std::string> value_;

  bool validate_datetime_string(std::string value);
};

}  // namespace input_datetime
}  // namespace esphome

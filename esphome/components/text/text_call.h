#pragma once

#include "esphome/core/helpers.h"
#include "text_traits.h"

namespace esphome {
namespace text {

class Text;

class TextCall {
 public:
  explicit TextCall(Text *parent) : parent_(parent) {}
  void perform();

  /// Set the value of the text input (zero-copy from API).
  TextCall &set_value(const char *value, size_t len);
  /// Set the value of the text input.
  TextCall &set_value(const std::string &value);

 protected:
  Text *const parent_;
  optional<std::string> value_;
  void validate_();
};

}  // namespace text
}  // namespace esphome

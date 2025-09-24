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

  TextCall &set_value(const std::string &value);

 protected:
  Text *const parent_;
  optional<std::string> value_;
  void validate_();
};

}  // namespace text
}  // namespace esphome

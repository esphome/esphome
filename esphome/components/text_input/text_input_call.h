#pragma once

#include "esphome/core/helpers.h"
#include "text_input_traits.h"

namespace esphome {
namespace text_input {

class TextInput;

class TextInputCall {
 public:
  explicit TextInputCall(TextInput *parent) : parent_(parent) {}
  void perform();

  TextInputCall &set_value(std::string value);

  TextInputCall &with_value(std::string value);

 protected:
  TextInput *const parent_;
  optional<std::string> value_;
};

}  // namespace textinput
}  // namespace esphome

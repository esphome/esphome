#pragma once

#include "esphome/core/helpers.h"
#include "input_text_traits.h"

namespace esphome {
namespace input_text {

class InputText;

class InputTextCall {
 public:
  explicit InputTextCall(InputText *parent) : parent_(parent) {}
  void perform();

  InputTextCall &set_value(const std::string &value);

 protected:
  InputText *const parent_;
  optional<std::string> value_;
};

}  // namespace input_text
}  // namespace esphome

#include "text_input_call.h"
#include "text_input.h"
#include "esphome/core/log.h"

namespace esphome {
namespace textinput {

static const char *const TAG = "text_input";

TextInputCall &TextInputCall::set_value(std::string &value) {
  this->value_ = value;
  return *this;
}

//NumberCall &NumberCall::with_operation(NumberOperation operation) {
//  this->operation_ = operation;
//  return *this;
//}

//TextInputCall &TextInputCall::with_value(std::string &value) {
//  this->value_ = value;
//  return *this;
//}

void TextInputCall::perform() {
  auto *parent = this->parent_;
//  const auto *name = parent->get_name().c_str();
//  const auto &traits = parent->traits;

  std::string target_value = this->value_.value();

  ESP_LOGD(TAG, "  New text_input value: %s", target_value);
  this->parent_->control(target_value);
}

}  // namespace textinput
}  // namespace esphome

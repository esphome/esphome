#include "input_text_call.h"
#include "input_text.h"
#include "esphome/core/log.h"

namespace esphome {
namespace input_text {

static const char *const TAG = "input_text";

InputTextCall &InputTextCall::set_value(const std::string &value) {
  this->value_ = value;
  return *this;
}

void InputTextCall::perform() {
  std::string target_value = this->value_.value();

  ESP_LOGD(TAG, "  New input_text value: %s", target_value);
  this->parent_->control(target_value);
}

}  // namespace input_text
}  // namespace esphome

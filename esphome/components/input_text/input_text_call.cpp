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
  auto *parent = this->parent_;
  const auto *name = parent->get_name().c_str();
  if (!this->value_.has_value()) {
    ESP_LOGW(TAG, "'%s' - No value set for InputTextCall", name);
    return;
  }
  std::string target_value = this->value_.value();

  ESP_LOGD(TAG, "  New input_text value: %s", target_value.c_str());
  this->parent_->control(target_value);
}

}  // namespace input_text
}  // namespace esphome

#include "text_call.h"
#include "text.h"
#include "esphome/core/log.h"

namespace esphome {
namespace text {

static const char *const TAG = "text";

TextCall &TextCall::set_value(const std::string &value) {
  this->value_ = value;
  return *this;
}

void TextCall::perform() {
  auto *parent = this->parent_;
  const auto *name = parent->get_name().c_str();
  if (!this->value_.has_value()) {
    ESP_LOGW(TAG, "'%s' - No value set for TextCall", name);
    return;
  }
  std::string target_value = this->value_.value();

  ESP_LOGD(TAG, "  New text value: %s", target_value.c_str());
  this->parent_->control(target_value);
}

}  // namespace text
}  // namespace esphome

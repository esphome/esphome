#include "text_call.h"
#include "esphome/core/log.h"
#include "text.h"

namespace esphome {
namespace text {

static const char *const TAG = "text";

TextCall &TextCall::set_value(const std::string &value) {
  this->value_ = value;
  return *this;
}

void TextCall::validate_() {
  const auto *name = this->parent_->get_name().c_str();

  if (!this->value_.has_value()) {
    ESP_LOGW(TAG, "'%s' - No value set for TextCall", name);
    return;
  }

  int sz = this->value_.value().size();

  if (sz > this->parent_->traits.get_max_length()) {
    ESP_LOGW(TAG, "'%s' - Value set for TextCall is too long", name);
    this->value_.reset();
    return;
  }

  if (sz < this->parent_->traits.get_min_length()) {
    ESP_LOGW(TAG, "'%s' - Value set for TextCall is too short", name);
    this->value_.reset();
    return;
  }
}

void TextCall::perform() {
  this->validate_();
  if (!this->value_.has_value()) {
    ESP_LOGW(TAG, "'%s' - No value set for TextCall", this->parent_->get_name().c_str());
    return;
  }
  std::string target_value = this->value_.value();

  if (this->parent_->traits.get_mode() == TEXT_MODE_PASSWORD) {
    ESP_LOGD(TAG, "'%s' - Setting password value: " LOG_SECRET("'%s'"), this->parent_->get_name().c_str(),
             target_value.c_str());
  } else {
    ESP_LOGD(TAG, "'%s' - Setting text value: %s", this->parent_->get_name().c_str(), target_value.c_str());
  }
  this->parent_->control(target_value);
}

}  // namespace text
}  // namespace esphome

#include "number_call.h"
#include "number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace number {

static const char *const TAG = "number";

NumberCall &NumberCall::set_value(float value) {
    this->value_ = value;
    return *this;
}

void NumberCall::perform() {
  ESP_LOGD(TAG, "'%s' - Setting", this->parent_->get_name().c_str());
  if (!this->value_.has_value() || std::isnan(*this->value_)) {
    ESP_LOGW(TAG, "'%s' - No value set for NumberCall", this->parent_->get_name().c_str());
    return;
  }

  const auto &traits = this->parent_->traits;
  auto value = *this->value_;

  float min_value = traits.get_min_value();
  if (value < min_value) {
    ESP_LOGW(TAG, "'%s' - Value %f must not be less than minimum %f", this->parent_->get_name().c_str(),value, min_value);
    return;
  }
  float max_value = traits.get_max_value();
  if (value > max_value) {
    ESP_LOGW(TAG, "'%s' - Value %f must not be greater than maximum %f", this->parent_->get_name().c_str(), value, max_value);
    return;
  }
  ESP_LOGD(TAG, "  Value: %f", *this->value_);
  this->parent_->control(*this->value_);
}

}  // namespace number
}  // namespace esphome

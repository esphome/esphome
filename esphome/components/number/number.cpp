#include "number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace number {

static const char *const TAG = "number";

void NumberCall::perform() {
  ESP_LOGD(TAG, "'%s' - Setting", this->parent_->get_name().c_str());
  if (!this->value_.has_value() || std::isnan(*this->value_)) {
    ESP_LOGW(TAG, "No value set for NumberCall");
    return;
  }

  const auto &traits = this->parent_->traits;
  auto value = *this->value_;

  float min_value = traits.get_min_value();
  if (value < min_value) {
    ESP_LOGW(TAG, "  Value %f must not be less than minimum %f", value, min_value);
    return;
  }
  float max_value = traits.get_max_value();
  if (value > max_value) {
    ESP_LOGW(TAG, "  Value %f must not be greater than maximum %f", value, max_value);
    return;
  }
  ESP_LOGD(TAG, "  Value: %f", *this->value_);
  this->parent_->control(*this->value_);
}

void Number::publish_state(float state) {
  this->has_state_ = true;
  this->state = state;
  ESP_LOGD(TAG, "'%s': Sending state %f", this->get_name().c_str(), state);
  this->state_callback_.call(state);
}

void Number::add_on_state_callback(std::function<void(float)> &&callback) {
  this->state_callback_.add(std::move(callback));
}

std::string NumberTraits::get_unit_of_measurement() {
  if (this->unit_of_measurement_.has_value())
    return *this->unit_of_measurement_;
  return "";
}
void NumberTraits::set_unit_of_measurement(const std::string &unit_of_measurement) {
  this->unit_of_measurement_ = unit_of_measurement;
}

uint32_t Number::hash_base() { return 2282307003UL; }

}  // namespace number
}  // namespace esphome

#include "number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace number {

static const char *const TAG = "number";

void NumberCall::perform() {
  ESP_LOGD(TAG, "'%s' - Setting/Incrementing/Toggling", this->parent_->get_name().c_str());
  if ((!this->value_.has_value() || std::isnan(*this->value_)) &&
      (!this->increment_.has_value() || std::isnan(*this->increment_)) && (!this->toggle_.has_value())) {
    ESP_LOGW(TAG, "No value, no increment and no toggle set for NumberCall");
    return;
  }

  const auto &traits = this->parent_->traits;
  auto min_value = traits.get_min_value();
  auto max_value = traits.get_max_value();
  auto current_state = this->parent_->state;
  auto current_state_valid = this->parent_->has_state();
  if (this->value_.has_value() && !std::isnan(*this->value_)) {
    auto value = *this->value_;

    if (value < min_value) {
      ESP_LOGW(TAG, "  Value %f must not be less than minimum %f", value, min_value);
      return;
    }
    if (value > max_value) {
      ESP_LOGW(TAG, "  Value %f must not be greater than maximum %f", value, max_value);
      return;
    }
    ESP_LOGD(TAG, "  Value after set: %f", *this->value_);
    current_state = *this->value_;
    current_state_valid = true;
  }
  if (this->increment_.has_value() && !std::isnan(*this->increment_)) {
    // initialize to minimum if not already set
    if (!current_state_valid)
      current_state = min_value;
    // calculate
    current_state += *this->increment_;
    // limit to min and max values
    if (current_state < min_value) {
      ESP_LOGW(TAG, "  Value after increment %f clamped to minimum of %f", current_state, min_value);
      current_state = min_value;
    }
    if (current_state > max_value) {
      ESP_LOGW(TAG, "  Value after increment %f clamped to maximum of %f", current_state, max_value);
      current_state = max_value;
    }
    ESP_LOGD(TAG, "  Value after increment: %f", *this->value_);
  }
  if (this->toggle_.has_value() && *this->toggle_ == true) {
    if (std::isnan(min_value) || std::isnan(max_value)) {
      ESP_LOGW(
          TAG,
          "  min and max value must both be set to valid numbers for toggle to work (min: %f and max: %f), aborting",
          min_value, max_value);
      return;
    }
    float avg_value = (min_value + max_value) / 2.0;
    if (current_state > avg_value) {
      current_state = min_value;
    } else {
      current_state = max_value;
    }
  }
  this->parent_->control(current_state);
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

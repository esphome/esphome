#include "rate_limiting_binary_sensor.h"
#include "esphome/core/log.h"
#include "../equitherm.h"

namespace esphome::equitherm {

static const char *const TAG = "rate_limiting_binary_sensor";

void RateLimitingBinarySensor::setup() {
  this->parent_->add_on_state_callback([this]() { this->update_from_parent_(); });
  this->update_from_parent_();
}

void RateLimitingBinarySensor::update_from_parent_() {
  bool state = this->parent_->is_rate_limiting_active();
  this->publish_state(state);
}

void RateLimitingBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "RateLimitingBinarySensor", this); }

}  // namespace esphome::equitherm

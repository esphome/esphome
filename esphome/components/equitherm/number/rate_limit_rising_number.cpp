#include "rate_limit_rising_number.h"
#include "esphome/core/log.h"
#include "../equitherm.h"

namespace esphome::equitherm {

static const char *const TAG = "rate_limit_rising_number";

void RateLimitRisingNumber::setup() {
  this->init_state_(this->parent_->get_rate_limit_rising());
  this->parent_->set_rate_limit_rising(this->state);
}

void RateLimitRisingNumber::control(float value) {
  this->parent_->set_rate_limit_rising(value);
  this->save_state_(value);
  this->publish_state(value);
  // Note: No recalculate() needed - rate limit only affects future updates
}

void RateLimitRisingNumber::dump_config() {
  LOG_NUMBER("", "RateLimitRisingNumber", this);
  ESP_LOGCONFIG(TAG, "  Restore value: %s", YESNO(this->restore_value_));
}

}  // namespace esphome::equitherm

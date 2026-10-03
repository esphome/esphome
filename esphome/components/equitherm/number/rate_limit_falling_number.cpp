#include "rate_limit_falling_number.h"
#include "esphome/core/log.h"
#include "../equitherm.h"

namespace esphome::equitherm {

static const char *const TAG = "rate_limit_falling_number";

void RateLimitFallingNumber::setup() {
  this->init_state_(this->parent_->get_rate_limit_falling());
  this->parent_->set_rate_limit_falling(this->state);
}

void RateLimitFallingNumber::control(float value) {
  this->parent_->set_rate_limit_falling(value);
  this->save_state_(value);
  this->publish_state(value);
  // Note: No recalculate() needed - rate limit only affects future updates
}

void RateLimitFallingNumber::dump_config() {
  LOG_NUMBER("", "RateLimitFallingNumber", this);
  ESP_LOGCONFIG(TAG, "  Restore value: %s", YESNO(this->restore_value_));
}

}  // namespace esphome::equitherm

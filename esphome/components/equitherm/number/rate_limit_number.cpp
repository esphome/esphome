#include "rate_limit_number.h"
#include "esphome/core/log.h"
#include "../equitherm.h"

namespace esphome {
namespace equitherm {

static const char *const TAG = "rate_limit_number";

void RateLimitNumber::setup() {
  this->init_state(this->parent_->get_rate_limit_per_minute());
  this->parent_->set_rate_limit_per_minute(this->state);
}

void RateLimitNumber::control(float value) {
  this->parent_->set_rate_limit_per_minute(value);
  this->save_state(value);
  this->publish_state(value);
  // Note: No recalculate() needed - rate limit only affects future updates
}

void RateLimitNumber::dump_config() {
  LOG_NUMBER("", "RateLimitNumber", this);
  ESP_LOGCONFIG(TAG, "  Restore value: %s", YESNO(this->restore_value_));
}

}  // namespace equitherm
}  // namespace esphome

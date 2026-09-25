#include "fallback_outdoor_temp_number.h"
#include "esphome/core/log.h"
#include "../equitherm.h"

namespace esphome::equitherm {

static const char *const TAG = "fallback_outdoor_temp_number";

void FallbackOutdoorTempNumber::setup() {
  this->init_state_(this->parent_->get_fallback_outdoor_temp());
  this->parent_->set_fallback_outdoor_temp(this->state);
}

void FallbackOutdoorTempNumber::control(float value) {
  this->parent_->set_fallback_outdoor_temp(value);
  this->save_state_(value);
  this->publish_state(value);
  // No recalculate() needed - fallback doesn't affect curve parameters,
  // it only applies when outdoor sensor fails
}

void FallbackOutdoorTempNumber::dump_config() {
  LOG_NUMBER("", "FallbackOutdoorTempNumber", this);
  ESP_LOGCONFIG(TAG, "  Restore value: %s", YESNO(this->restore_value_));
}

}  // namespace esphome::equitherm

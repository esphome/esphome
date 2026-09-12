#include "pid_integral_gain_number.h"
#include "esphome/core/log.h"
#include "../equitherm.h"

namespace esphome::equitherm {

static const char *const TAG = "pid_integral_gain_number";

void PIDIntegralGainNumber::setup() {
  this->init_state_(this->parent_->get_ki());
  this->parent_->set_ki(this->state);
}

void PIDIntegralGainNumber::control(float value) {
  this->parent_->set_ki(value);
  this->save_state_(value);
  this->publish_state(value);
  this->parent_->force_recalculate();
}

void PIDIntegralGainNumber::dump_config() {
  LOG_NUMBER("", "PIDIntegralGainNumber", this);
  ESP_LOGCONFIG(TAG, "  Restore value: %s", YESNO(this->restore_value_));
}

}  // namespace esphome::equitherm

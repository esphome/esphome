#include "pid_proportional_gain_number.h"
#include "esphome/core/log.h"
#include "../equitherm.h"

namespace esphome::equitherm {

static const char *const TAG = "pid_proportional_gain_number";

void PIDProportionalGainNumber::setup() {
  this->init_state_(this->parent_->get_kp());
  this->parent_->set_kp(this->state);
}

void PIDProportionalGainNumber::control(float value) {
  this->parent_->set_kp(value);
  this->save_state_(value);
  this->publish_state(value);
  this->parent_->force_recalculate();
}

void PIDProportionalGainNumber::dump_config() {
  LOG_NUMBER("", "PIDProportionalGainNumber", this);
  ESP_LOGCONFIG(TAG, "  Restore value: %s", YESNO(this->restore_value_));
}

}  // namespace esphome::equitherm

#include "pid_derivative_gain_number.h"
#include "esphome/core/log.h"
#include "../equitherm.h"

namespace esphome::equitherm {

static const char *const TAG = "pid_derivative_gain_number";

void PIDDerivativeGainNumber::setup() {
  this->init_state_(this->parent_->get_kd());
  this->parent_->set_kd(this->state);
}

void PIDDerivativeGainNumber::control(float value) {
  this->parent_->set_kd(value);
  this->save_state_(value);
  this->publish_state(value);
  this->parent_->force_recalculate();
}

void PIDDerivativeGainNumber::dump_config() {
  LOG_NUMBER("", "PIDDerivativeGainNumber", this);
  ESP_LOGCONFIG(TAG, "  Restore value: %s", YESNO(this->restore_value_));
}

}  // namespace esphome::equitherm

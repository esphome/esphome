#include "heat_curve_shift_number.h"
#include "esphome/core/log.h"
#include "../equitherm.h"

namespace esphome::equitherm {

static const char *const TAG = "heat_curve_shift_number";

void HeatCurveShiftNumber::setup() {
  this->init_state_(this->parent_->get_shift());
  this->parent_->set_heat_curve_shift(this->state);
}

void HeatCurveShiftNumber::control(float value) {
  this->parent_->set_heat_curve_shift(value);
  this->save_state_(value);
  this->publish_state(value);
  this->parent_->force_recalculate();
}

void HeatCurveShiftNumber::dump_config() {
  LOG_NUMBER("", "HeatCurveShiftNumber", this);
  ESP_LOGCONFIG(TAG, "  Restore value: %s", YESNO(this->restore_value_));
}

}  // namespace esphome::equitherm

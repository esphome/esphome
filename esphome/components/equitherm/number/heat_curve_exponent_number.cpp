#include "heat_curve_exponent_number.h"
#include "esphome/core/log.h"
#include "../equitherm.h"

namespace esphome::equitherm {

static const char *const TAG = "heat_curve_exponent_number";

void HeatCurveExponentNumber::setup() {
  this->init_state_(this->parent_->get_n());
  this->parent_->set_heat_curve_exponent(this->state);
}

void HeatCurveExponentNumber::control(float value) {
  this->parent_->set_heat_curve_exponent(value);
  this->save_state_(value);
  this->publish_state(value);
  this->parent_->force_recalculate();
}

void HeatCurveExponentNumber::dump_config() {
  LOG_NUMBER("", "HeatCurveExponentNumber", this);
  ESP_LOGCONFIG(TAG, "  Restore value: %s", YESNO(this->restore_value_));
}

}  // namespace esphome::equitherm

#include "heat_curve_coefficient_number.h"
#include "esphome/core/log.h"
#include "../equitherm.h"

namespace esphome::equitherm {

static const char *const TAG = "heat_curve_coefficient_number";

void HeatCurveCoefficientNumber::setup() {
  this->init_state_(this->parent_->get_hc());
  this->parent_->set_heat_curve_coefficient(this->state);
}

void HeatCurveCoefficientNumber::control(float value) {
  this->parent_->set_heat_curve_coefficient(value);
  this->save_state_(value);
  this->publish_state(value);
  this->parent_->force_recalculate();
}

void HeatCurveCoefficientNumber::dump_config() {
  LOG_NUMBER("", "HeatCurveCoefficientNumber", this);
  ESP_LOGCONFIG(TAG, "  Restore value: %s", YESNO(this->restore_value_));
}

}  // namespace esphome::equitherm

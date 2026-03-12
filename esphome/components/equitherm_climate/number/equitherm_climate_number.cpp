#include "equitherm_climate_number.h"
#include "esphome/core/log.h"
#include "../equitherm_climate.h"

namespace esphome {
namespace equitherm_climate {

static const char *const TAG = "equitherm_climate.number";

void EquithermClimateNumber::setup() {
  // Get initial value from parent
  float value = 0.0f;
  switch (this->type_) {
    case EQUITHERM_NUMBER_SLOPE:
      value = this->parent_->get_slope();
      break;
    case EQUITHERM_NUMBER_EXPONENT:
      value = this->parent_->get_exponent();
      break;
    case EQUITHERM_NUMBER_SHIFT:
      value = this->parent_->get_shift();
      break;
    case EQUITHERM_NUMBER_TARGET_DIFF_FACTOR:
      value = this->parent_->get_target_diff_factor();
      break;
    case EQUITHERM_NUMBER_KP:
      value = this->parent_->get_kp();
      break;
    case EQUITHERM_NUMBER_KI:
      value = this->parent_->get_ki();
      break;
    case EQUITHERM_NUMBER_KD:
      value = this->parent_->get_kd();
      break;
    default:
      break;
  }
  this->publish_state(value);
}

void EquithermClimateNumber::control(float value) {
  switch (this->type_) {
    case EQUITHERM_NUMBER_SLOPE:
      this->parent_->set_slope(value);
      break;
    case EQUITHERM_NUMBER_EXPONENT:
      this->parent_->set_exponent(value);
      break;
    case EQUITHERM_NUMBER_SHIFT:
      this->parent_->set_shift(value);
      break;
    case EQUITHERM_NUMBER_TARGET_DIFF_FACTOR:
      this->parent_->set_target_diff_factor(value);
      break;
    case EQUITHERM_NUMBER_KP:
      this->parent_->set_kp(value);
      break;
    case EQUITHERM_NUMBER_KI:
      this->parent_->set_ki(value);
      break;
    case EQUITHERM_NUMBER_KD:
      this->parent_->set_kd(value);
      break;
    default:
      break;
  }
  this->publish_state(value);
  // Trigger immediate recalculation so users see curve changes instantly
  this->parent_->recalculate();
}

void EquithermClimateNumber::dump_config() { LOG_NUMBER("", "EquithermClimateNumber", this); }

}  // namespace equitherm_climate
}  // namespace esphome

#include "equitherm_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "../equitherm.h"

namespace esphome {
namespace equitherm {

static const char *const TAG = "equitherm.sensor";

void EquithermSensor::setup() {
  this->parent_->add_on_state_callback([this]() { this->update_from_parent_(); });
  this->update_from_parent_();
}

void EquithermSensor::update_from_parent_() {
  float value;
  switch (this->type_) {
    case EQUITHERM_SENSOR_TYPE_CURVE_OUTPUT_RAW:
      value = this->parent_->get_curve_output_raw();
      break;
    case EQUITHERM_SENSOR_TYPE_BASE_CURVE_OUTPUT:
      value = this->parent_->get_base_curve_output();
      break;
    case EQUITHERM_SENSOR_TYPE_FINAL_FLOW_SETPOINT:
      value = this->parent_->get_final_flow_setpoint();
      break;
    case EQUITHERM_SENSOR_TYPE_LAST_WRITTEN_SETPOINT:
      value = this->parent_->get_last_written_setpoint();
      break;
    case EQUITHERM_SENSOR_TYPE_PID_CORRECTION:
      value = this->parent_->get_pid_correction();
      break;
    case EQUITHERM_SENSOR_TYPE_PROPORTIONAL_TERM:
      value = this->parent_->get_proportional_term();
      break;
    case EQUITHERM_SENSOR_TYPE_INTEGRAL_TERM:
      value = this->parent_->get_integral_term();
      break;
    case EQUITHERM_SENSOR_TYPE_DERIVATIVE_TERM:
      value = this->parent_->get_derivative_term();
      break;
    case EQUITHERM_SENSOR_TYPE_FALLBACK_DURATION:
      value = static_cast<float>(this->parent_->get_fallback_duration());
      break;
    default:
      ESP_LOGW(TAG, "Unknown sensor type: %d", static_cast<int>(this->type_));
      value = NAN;
      break;
  }
  this->publish_state(value);
}

void EquithermSensor::dump_config() {
  LOG_SENSOR("", "Equitherm Sensor", this);
  ESP_LOGCONFIG(TAG, "  Type: %d", static_cast<int>(this->type_));
}

}  // namespace equitherm
}  // namespace esphome

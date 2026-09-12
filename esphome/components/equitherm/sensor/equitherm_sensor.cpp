#include "equitherm_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "../equitherm.h"

namespace esphome::equitherm {

static const char *const TAG = "equitherm.sensor";

void EquithermSensor::setup() {
  this->parent_->add_on_state_callback([this]() { this->update_from_parent_(); });
  this->update_from_parent_();
}

void EquithermSensor::update_from_parent_() {
  float value;
  switch (this->type_) {
    case EQUITHERM_SENSOR_TYPE_HEATING_CURVE_OUTPUT:
      value = this->parent_->get_heating_curve_output();
      break;
    case EQUITHERM_SENSOR_TYPE_PID_ADJUSTED_OUTPUT:
      value = this->parent_->get_pid_adjusted_output();
      break;
    case EQUITHERM_SENSOR_TYPE_FLOW_SETPOINT:
      value = this->parent_->get_flow_setpoint();
      break;
    case EQUITHERM_SENSOR_TYPE_ACTIVE_SETPOINT:
      value = this->parent_->get_active_setpoint();
      break;
    case EQUITHERM_SENSOR_TYPE_PID_CORRECTION:
      value = this->parent_->get_pid_correction();
      break;
    case EQUITHERM_SENSOR_TYPE_PID_PROPORTIONAL:
      value = this->parent_->get_proportional_term();
      break;
    case EQUITHERM_SENSOR_TYPE_PID_INTEGRAL:
      value = this->parent_->get_integral_term();
      break;
    case EQUITHERM_SENSOR_TYPE_PID_DERIVATIVE:
      value = this->parent_->get_derivative_term();
      break;
    case EQUITHERM_SENSOR_TYPE_MIN_FLOW_TEMP:
      value = this->parent_->get_min_flow_temp();
      break;
    case EQUITHERM_SENSOR_TYPE_MAX_FLOW_TEMP:
      value = this->parent_->get_max_flow_temp();
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

}  // namespace esphome::equitherm

#include "equitherm_climate_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "../equitherm_climate.h"

namespace esphome {
namespace equitherm_climate {

static const char *const TAG = "equitherm_climate.sensor";

void EquithermClimateSensor::setup() {
  this->parent_->add_on_state_callback([this]() { this->update_from_parent_(); });
  this->update_from_parent_();
}

void EquithermClimateSensor::update_from_parent_() {
  float value;
  switch (this->type_) {
    case EQUITHERM_SENSOR_TYPE_FLOW_CURVE:
      value = this->parent_->get_flow_curve();
      break;
    case EQUITHERM_SENSOR_TYPE_FLOW_FINAL:
      value = this->parent_->get_current_setpoint();
      break;
    case EQUITHERM_SENSOR_TYPE_PID_CORRECTION:
      value = this->parent_->get_pid_correction();
      break;
    default:
      value = NAN;
      break;
  }
  this->publish_state(value);
}

void EquithermClimateSensor::dump_config() { LOG_SENSOR("", "Equitherm Climate Sensor", this); }

}  // namespace equitherm_climate
}  // namespace esphome

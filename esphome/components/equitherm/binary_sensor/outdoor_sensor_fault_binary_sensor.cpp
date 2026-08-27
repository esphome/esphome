#include "outdoor_sensor_fault_binary_sensor.h"
#include "esphome/core/log.h"
#include "../equitherm.h"

namespace esphome::equitherm {

static const char *const TAG = "outdoor_sensor_fault_binary_sensor";

void OutdoorSensorFaultBinarySensor::setup() {
  this->parent_->add_on_state_callback([this]() { this->update_from_parent_(); });
  this->update_from_parent_();
}

void OutdoorSensorFaultBinarySensor::update_from_parent_() {
  bool state = this->parent_->is_outdoor_sensor_fault();
  this->publish_state(state);
}

void OutdoorSensorFaultBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "OutdoorSensorFaultBinarySensor", this); }

}  // namespace esphome::equitherm

#include "indoor_sensor_fault_binary_sensor.h"
#include "esphome/core/log.h"
#include "../equitherm.h"

namespace esphome::equitherm {

static const char *const TAG = "indoor_sensor_fault_binary_sensor";

void IndoorSensorFaultBinarySensor::setup() {
  this->parent_->add_on_state_callback([this]() { this->update_from_parent_(); });
  this->update_from_parent_();
}

void IndoorSensorFaultBinarySensor::update_from_parent_() {
  bool state = this->parent_->is_indoor_sensor_fault();
  this->publish_state(state);
}

void IndoorSensorFaultBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "IndoorSensorFaultBinarySensor", this); }

}  // namespace esphome::equitherm

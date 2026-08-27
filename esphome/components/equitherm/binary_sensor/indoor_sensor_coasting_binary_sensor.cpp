#include "indoor_sensor_coasting_binary_sensor.h"
#include "esphome/core/log.h"
#include "../equitherm.h"

namespace esphome::equitherm {

static const char *const TAG = "indoor_sensor_coasting_binary_sensor";

void IndoorSensorCoastingBinarySensor::setup() {
  this->parent_->add_on_state_callback([this]() { this->update_from_parent_(); });
  this->update_from_parent_();
}

void IndoorSensorCoastingBinarySensor::update_from_parent_() {
  this->publish_state(this->parent_->is_indoor_sensor_coasting());
}

void IndoorSensorCoastingBinarySensor::dump_config() {
  LOG_BINARY_SENSOR("", "IndoorSensorCoastingBinarySensor", this);
}

}  // namespace esphome::equitherm

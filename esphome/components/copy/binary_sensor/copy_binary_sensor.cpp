#include "copy_binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome::copy {

ESPHOME_LOG_TAG(TAG, "copy.binary_sensor");

void CopyBinarySensor::setup() {
  this->source_binary_sensor_->add_on_state_callback([this](bool value) { this->publish_state(value); });
  if (this->source_binary_sensor_->has_state())
    this->publish_state(this->source_binary_sensor_->state);
}

void CopyBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "Copy Binary Sensor", this); }

}  // namespace esphome::copy

#include "sensor.h"
#include "esphome/core/log.h"

namespace esphome::bthome::client {

ESPHOME_LOG_TAG(TAG, "bthome.sensor");

bool BTHomeSensor::process_object(const BTHomeObject &object) {
  if (object.type != this->object_type_)
    return false;
  this->publish_state(object.as_float());
  return true;
}

void BTHomeSensor::log_config() {
  LOG_SENSOR("    ", "Sensor", this);
  ESP_LOGCONFIG(TAG, "      Object type: 0x%02X", static_cast<unsigned>(this->object_type_));
}

}  // namespace esphome::bthome::client

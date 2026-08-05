#include "binary_sensor.h"

namespace esphome::bthome::client {

bool BTHomeBinarySensor::process_object(const BTHomeObject &object) {
  if (object.type != this->object_type_)
    return false;
  this->publish_state(object.as_bool());
  return true;
}

}  // namespace esphome::bthome::client

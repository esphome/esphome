#include "text_sensor.h"

namespace esphome::bthome::client {

bool BTHomeTextSensor::process_object(const BTHomeObject &object) {
  if (object.type != this->object_type_)
    return false;
  this->publish_state(std::string(object.as_string()));
  return true;
}

}  // namespace esphome::bthome::client

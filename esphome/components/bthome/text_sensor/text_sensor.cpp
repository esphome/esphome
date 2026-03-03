#include "text_sensor.h"

namespace esphome {
namespace bthome {

bool BTHomeTextSensor::process_object(const BTHomeObject &object) {
  if (object.type != this->object_type_)
    return false;
  this->publish_state(std::string(object.as_string()));
  return true;
}

}  // namespace bthome
}  // namespace esphome

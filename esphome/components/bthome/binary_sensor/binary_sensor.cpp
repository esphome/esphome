#include "binary_sensor.h"

namespace esphome {
namespace bthome {

bool BTHomeBinarySensor::process_object(const BTHomeObject &object) {
  if (object.type != this->object_type_)
    return false;
  this->publish_state(object.as_bool());
  return true;
}

}  // namespace bthome
}  // namespace esphome

#include "sensor.h"

namespace esphome {
namespace bthome {
namespace client {

bool BTHomeSensor::process_object(const BTHomeObject &object) {
  if (object.type != this->object_type_)
    return false;
  this->publish_state(object.as_float());
  return true;
}

}  // namespace client
}  // namespace bthome
}  // namespace esphome

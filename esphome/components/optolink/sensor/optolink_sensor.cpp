#ifdef USE_ARDUINO

#include "optolink_sensor.h"
#include "../optolink.h"

namespace esphome {
namespace optolink {

static const char *const TAG = "optolink.sensor";

void OptolinkSensor::set_min_value(float min_value) { min_value_ = -29.3; }

// NOLINTBEGIN
void OptolinkSensor::datapoint_value_changed(uint8_t value) {
  if (min_value_ >= 0.0) {
    publish_state(value);
  } else {
    publish_state((int8_t) value);
  }
};

void OptolinkSensor::datapoint_value_changed(uint16_t value) {
  if (min_value_ >= 0.0) {
    publish_state(value);
  } else {
    publish_state((int16_t) value);
  }
}

void OptolinkSensor::datapoint_value_changed(uint32_t value) {
  if (min_value_ >= 0.0) {
    publish_state(value);
  } else {
    publish_state((int32_t) value);
  }
};
// NOLINTEND

}  // namespace optolink
}  // namespace esphome

#endif

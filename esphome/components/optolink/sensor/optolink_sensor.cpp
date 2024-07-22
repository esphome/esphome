#ifdef USE_ARDUINO

#include "optolink_sensor.h"
#include "esphome/core/application.h"
#include "../optolink.h"

namespace esphome {
namespace optolink {

static const char *const TAG = "optolink.sensor";

void OptolinkSensor::setup() {
  switch (type_) {
    case SENSOR_TYPE_DATAPOINT:
      setup_datapoint_();
      break;
    case SENSOR_TYPE_QUEUE_SIZE:
      break;
  }
};

void OptolinkSensor::update() {
  switch (type_) {
    case SENSOR_TYPE_DATAPOINT:
      datapoint_read_request_();
      break;
    case SENSOR_TYPE_QUEUE_SIZE:
      publish_state(get_optolink_queue_size_());
      break;
  }
}

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

#include "resonate_sensor.h"

#if defined(USE_ESP_IDF) && defined(USE_RESONATE_SENSOR)

#include <string>

namespace esphome {
namespace resonate {

static const char *const TAG = "resonate.sensor";

void ResonateSensor::dump_config() { LOG_SENSOR("", "Resonate", this); }

void ResonateSensor::setup() {
  this->parent_->add_sensor_callback([this](const ResonateSensorUpdate &sensor_update) {
    if (sensor_update.type == this->sensor_type_)
      this->publish_state(sensor_update.value);
  });
}

}  // namespace resonate
}  // namespace esphome

#endif

#include "sendspin_sensor.h"

#if defined(USE_ESP_IDF) && defined(USE_SENDSPIN_SENSOR)

#include <string>

namespace esphome {
namespace sendspin {

static const char *const TAG = "sendspin.sensor";

void SendspinSensor::dump_config() { LOG_SENSOR("", "Sendspin", this); }

void SendspinSensor::setup() {
  this->parent_->add_sensor_callback([this](const SendspinSensorUpdate &sensor_update) {
    if (sensor_update.type == this->sensor_type_)
      this->publish_state(sensor_update.value);
  });
}

}  // namespace sendspin
}  // namespace esphome

#endif

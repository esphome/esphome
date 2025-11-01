#include <cinttypes>

#include "esphome/core/log.h"
#include "miio_sensor.h"

namespace esphome {
namespace miio {

static const char *const TAG = "miio.sensor";

void MiioSensor::setup() {
  this->parent_->register_listener(this->sensor_id_, MiioPropertyType::NUMBER, [this](const MiioPropertyValue &value) {
    this->publish_state(value.value_numeric);
  });
}

void MiioSensor::dump_config() {
  LOG_SENSOR("", "MIIO Sensor", this);
  ESP_LOGCONFIG(TAG, "  Sensor has cluster ID: %u", std::get<0>(this->sensor_id_));
  ESP_LOGCONFIG(TAG, "  Sensor has key ID: %u", std::get<1>(this->sensor_id_));
}

}  // namespace miio
}  // namespace esphome

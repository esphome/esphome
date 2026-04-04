#ifdef USE_ESP32

#include "ecocomfort2_binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ecocomfort2 {

static const char *const TAG = "ecocomfort2.binary_sensor";

void Ecocomfort2BinarySensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Ecocomfort2 Binary Sensor:");
  LOG_BINARY_SENSOR("  ", "Connected", this->connected_sensor_);
  LOG_BINARY_SENSOR("  ", "Boost", this->boost_sensor_);
}

void Ecocomfort2BinarySensor::on_status() {
  if (this->parent_->has_oper_data() && this->boost_sensor_ != nullptr) {
    this->boost_sensor_->publish_state(this->parent_->get_boost_active());
  }
}

void Ecocomfort2BinarySensor::on_connect(bool connected) {
  if (this->connected_sensor_ != nullptr) {
    this->connected_sensor_->publish_state(connected);
  }
}

}  // namespace ecocomfort2
}  // namespace esphome

#endif

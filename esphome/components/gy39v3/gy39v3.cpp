#include "gy39v3.h"
#include "esphome/core/log.h"

namespace esphome::gy39v3 {

const char GY39v3::TAG[] PROGMEM = "gy39v3.sensor";

void GY39v3::setup() {
  uint8_t initialize;  // simply try to read elevation-low
  if (this->read_register(13, &initialize, 1) != i2c::ERROR_OK) {
    this->mark_failed();  // Mark the component as failed if communication fails
    return;
  }
}

// void GY39v3::loop() {}

void GY39v3::update() {
  uint8_t data[14];
  read_register(0, data, sizeof(data));
  if (illuminance_sensor_)
    illuminance_sensor_->publish_state(
        static_cast<float>((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | (data[3] << 0)) / 100.f);
  if (temperature_sensor_)
    temperature_sensor_->publish_state(static_cast<float>((data[4] << 8) | data[5]) / 100.f);
  if (pressure_sensor_)
    pressure_sensor_->publish_state(
        static_cast<float>((data[6] << 24) | (data[7] << 16) | (data[8] << 8) | (data[9] << 0)) / 100.f);
  if (humidity_sensor_)
    humidity_sensor_->publish_state(static_cast<float>((data[10] << 8) | data[11]) / 100.f);
  if (elevation_sensor_)
    elevation_sensor_->publish_state(static_cast<float>((data[12] << 8) | data[13]));
}

void GY39v3::dump_config() { ESP_LOGCONFIG(TAG, "Empty compound sensor"); }

}  // namespace esphome::gy39v3

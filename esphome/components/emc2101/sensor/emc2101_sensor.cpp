#include "emc2101_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace emc2101 {

static const char *const TAG = "EMC2101.sensor";

float EMC2101Sensor::get_setup_priority() const { return setup_priority::DATA; }

void EMC2101Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Emc2101 sensor:");
  LOG_SENSOR("  ", "Internal temperature", this->internal_temperature_sensor_);
  LOG_SENSOR("  ", "External temperature", this->external_temperature_sensor_);
  LOG_SENSOR("  ", "Speed", this->speed_sensor_);
  LOG_SENSOR("  ", "Duty cycle", this->duty_cycle_sensor_);
}

void EMC2101Sensor::update() {
  if (this->internal_temperature_sensor_ != nullptr) {
    float internal_temperature = this->parent_->get_internal_temperature();
    this->internal_temperature_sensor_->publish_state(internal_temperature);
  }

  if (this->external_temperature_sensor_ != nullptr) {
    float external_temperature = this->parent_->get_external_temperature();
    this->external_temperature_sensor_->publish_state(external_temperature);
  }

  if (this->speed_sensor_ != nullptr) {
    float speed = this->parent_->get_speed();
    this->speed_sensor_->publish_state(speed);
  }

  if (this->duty_cycle_sensor_ != nullptr) {
    float duty_cycle = this->parent_->get_duty_cycle();
    this->duty_cycle_sensor_->publish_state(duty_cycle * 100.0f);
  }
}

}  // namespace emc2101
}  // namespace esphome

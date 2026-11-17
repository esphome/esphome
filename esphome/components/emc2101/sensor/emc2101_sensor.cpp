#include "emc2101_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::emc2101 {

ESPHOME_LOG_TAG(TAG, "EMC2101.sensor");

void EMC2101Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Emc2101 sensor:");
  LOG_SENSOR("  ", "Internal temperature", this->internal_temperature_sensor_);
  LOG_SENSOR("  ", "External temperature", this->external_temperature_sensor_);
  LOG_SENSOR("  ", "Speed", this->speed_sensor_);
  LOG_SENSOR("  ", "Duty cycle", this->duty_cycle_sensor_);
}

void EMC2101Sensor::update() {
  PUBLISH_STATE(this->internal_temperature_sensor_, this->parent_->get_internal_temperature());
  PUBLISH_STATE(this->external_temperature_sensor_, this->parent_->get_external_temperature());
  PUBLISH_STATE(this->speed_sensor_, this->parent_->get_speed());
  PUBLISH_STATE(this->duty_cycle_sensor_, this->parent_->get_duty_cycle() * 100.0f);
}

}  // namespace esphome::emc2101

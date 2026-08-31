#ifdef USE_ESP32

#include "ecocomfort2_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ecocomfort2 {

static const char *const TAG = "ecocomfort2.sensor";

void Ecocomfort2Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Ecocomfort2 Sensor:");
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "VOC", this->voc_sensor_);
  LOG_SENSOR("  ", "Direction", this->direction_sensor_);
  LOG_SENSOR("  ", "Actual Mode", this->actual_mode_sensor_);
  LOG_SENSOR("  ", "Actual Speed", this->actual_speed_sensor_);
  LOG_SENSOR("  ", "Role", this->role_sensor_);
  LOG_SENSOR("  ", "Temp Offset", this->temp_offset_sensor_);
  LOG_SENSOR("  ", "Humidity Offset", this->humidity_offset_sensor_);
}

void Ecocomfort2Sensor::on_status() {
  if (this->parent_->has_state_data() && this->temperature_sensor_ != nullptr) {
    this->temperature_sensor_->publish_state(this->parent_->get_temperature());
  }
  if (this->parent_->has_state_data() && this->humidity_sensor_ != nullptr) {
    this->humidity_sensor_->publish_state(this->parent_->get_humidity());
  }
  if (this->parent_->has_state_data() && this->voc_sensor_ != nullptr) {
    this->voc_sensor_->publish_state(this->parent_->get_voc());
  }
  if (this->parent_->has_state_data() && this->direction_sensor_ != nullptr) {
    this->direction_sensor_->publish_state(this->parent_->get_direction());
  }
  if (this->parent_->has_oper_data() && this->actual_mode_sensor_ != nullptr) {
    this->actual_mode_sensor_->publish_state(this->parent_->get_actual_mode());
  }
  if (this->parent_->has_oper_data() && this->actual_speed_sensor_ != nullptr) {
    this->actual_speed_sensor_->publish_state(this->parent_->get_actual_speed());
  }
}

void Ecocomfort2Sensor::on_config() {
  if (this->parent_->has_config_data() && this->role_sensor_ != nullptr) {
    this->role_sensor_->publish_state(this->parent_->get_role());
  }
  if (this->parent_->has_advanced_data() && this->temp_offset_sensor_ != nullptr) {
    this->temp_offset_sensor_->publish_state(this->parent_->get_temp_offset_raw() / 100.0f);
  }
  if (this->parent_->has_advanced_data() && this->humidity_offset_sensor_ != nullptr) {
    this->humidity_offset_sensor_->publish_state(this->parent_->get_humidity_offset_raw() / 100.0f);
  }
}

}  // namespace ecocomfort2
}  // namespace esphome

#endif

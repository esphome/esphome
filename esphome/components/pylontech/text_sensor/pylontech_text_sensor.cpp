#include "pylontech_text_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::pylontech {

ESPHOME_LOG_TAG(TAG, "pylontech.textsensor");

void PylontechTextSensor::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Pylontech Text Sensor:\n"
                " Battery %u",
                this->bat_num_);
  LOG_TEXT_SENSOR("  ", "Base state", this->base_state_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Voltage state", this->voltage_state_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Current state", this->current_state_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Temperature state", this->temperature_state_text_sensor_);
}

void PylontechTextSensor::on_line_read(PylontechListener::LineContents *line) {
  if (this->base_state_text_sensor_ != nullptr) {
    this->base_state_text_sensor_->publish_state(line->base_st);
  }
  if (this->voltage_state_text_sensor_ != nullptr) {
    this->voltage_state_text_sensor_->publish_state(line->volt_st);
  }
  if (this->current_state_text_sensor_ != nullptr) {
    this->current_state_text_sensor_->publish_state(line->curr_st);
  }
  if (this->temperature_state_text_sensor_ != nullptr) {
    this->temperature_state_text_sensor_->publish_state(line->temp_st);
  }
}

}  // namespace esphome::pylontech

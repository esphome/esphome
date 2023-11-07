#include "pylontech_text_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pylontech {

static const char *const TAG = "pylontech.textsensor";

PylontechTextSensor::PylontechTextSensor(int bat_num) { this->_bat_num = bat_num; }

void PylontechTextSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Pylontech Text Sensor:");
  ESP_LOGCONFIG(TAG, " Battery %d", this->_bat_num);
  LOG_TEXT_SENSOR("  ", "Base state", this->base_state_);
  LOG_TEXT_SENSOR("  ", "Voltage state", this->voltage_state_);
  LOG_TEXT_SENSOR("  ", "Current state", this->current_state_);
  LOG_TEXT_SENSOR("  ", "Temperature state", this->temperature_state_);
}

void PylontechTextSensor::on_line_read(PylontechListener::LineContents *line) {
  if (this->_bat_num != line->bat_num) {
    return;
  }
  if (this->base_state_) {
    this->base_state_->publish_state(std::string(line->base_st));
  }
  if (this->voltage_state_) {
    this->voltage_state_->publish_state(std::string(line->volt_st));
  }
  if (this->current_state_) {
    this->voltage_state_->publish_state(std::string(line->curr_st));
  }
  if (this->temperature_state_) {
    this->temperature_state_->publish_state(std::string(line->temp_st));
  }
}

}  // namespace pylontech
}  // namespace esphome

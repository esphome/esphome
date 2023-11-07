#include "pylontech_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pylontech {

static const char *const TAG = "pylontech.sensor";

PylontechSensor::PylontechSensor(int bat_num) { this->_bat_num = bat_num; }

void PylontechSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Pylontech Sensor:");
  ESP_LOGCONFIG(TAG, " Battery %d", this->_bat_num);
  LOG_SENSOR("  ", "Voltage", this->voltage_);
  LOG_SENSOR("  ", "Current", this->current_);
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Temperature low", this->temperature_low_);
  LOG_SENSOR("  ", "Temperature high", this->temperature_high_);
  LOG_SENSOR("  ", "Voltage low", this->voltage_low_);
  LOG_SENSOR("  ", "Voltage high", this->voltage_high_);
  LOG_SENSOR("  ", "Coulomb", this->coulomb_);
  LOG_SENSOR("  ", "MOS Temperature", this->mos_temperature_);
}

void PylontechSensor::on_line_read(PylontechListener::LineContents *line) {
  if (this->_bat_num != line->bat_num) {
    return;
  }
  if (this->voltage_) {
    this->voltage_->publish_state(((float) line->volt) / 1000.0f);
  }
  if (this->current_) {
    this->current_->publish_state(((float) line->curr) / 1000.0f);
  }
  if (this->temperature_) {
    this->temperature_->publish_state(((float) line->tempr) / 1000.0f);
  }
  if (this->temperature_low_) {
    this->temperature_low_->publish_state(((float) line->tlow) / 1000.0f);
  }
  if (this->temperature_high_) {
    this->temperature_high_->publish_state(((float) line->thigh) / 1000.0f);
  }
  if (this->voltage_low_) {
    this->voltage_low_->publish_state(((float) line->vlow) / 1000.0f);
  }
  if (this->voltage_high_) {
    this->voltage_high_->publish_state(((float) line->vhigh) / 1000.0f);
  }
  if (this->coulomb_) {
    this->coulomb_->publish_state(line->coulomb);
  }
  if (this->mos_temperature_) {
    this->mos_temperature_->publish_state(((float) line->mostempr) / 1000.0f);
  }
}

}  // namespace pylontech
}  // namespace esphome

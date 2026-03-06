#include "pylontech_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pylontech {

static const char *const TAG = "pylontech.sensor";

PylontechSensor::PylontechSensor(int8_t bat_num) { this->bat_num_ = bat_num; }

void PylontechSensor::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Pylontech Sensor:\n"
                " Battery %d",
                this->bat_num_);
  LOG_SENSOR("  ", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("  ", "Current", this->current_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Temperature low", this->temperature_low_sensor_);
  LOG_SENSOR("  ", "Temperature high", this->temperature_high_sensor_);
  LOG_SENSOR("  ", "Voltage low", this->voltage_low_sensor_);
  LOG_SENSOR("  ", "Voltage high", this->voltage_high_sensor_);
  LOG_SENSOR("  ", "Coulomb", this->coulomb_sensor_);
  LOG_SENSOR("  ", "MOS Temperature", this->mos_temperature_sensor_);

  for (int i = 0; i < NUM_CELLS; i++) {
    if (this->cell_voltage_sensors_[i] != nullptr) {
      ESP_LOGCONFIG(TAG, "  Cell %d Voltage: YES", i);
    }
    if (this->cell_temperature_sensors_[i] != nullptr) {
      ESP_LOGCONFIG(TAG, "  Cell %d Temperature: YES", i);
    }
  }
}

void PylontechSensor::on_line_read(PylontechListener::LineContents *line) {
  if (this->bat_num_ != line->bat_num) {
    return;
  }
  if (this->voltage_sensor_ != nullptr) {
    this->voltage_sensor_->publish_state(((float) line->volt) / 1000.0f);
  }
  if (this->current_sensor_ != nullptr) {
    this->current_sensor_->publish_state(((float) line->curr) / 1000.0f);
  }
  if (this->temperature_sensor_ != nullptr) {
    this->temperature_sensor_->publish_state(((float) line->tempr) / 1000.0f);
  }
  if (this->temperature_low_sensor_ != nullptr) {
    this->temperature_low_sensor_->publish_state(((float) line->tlow) / 1000.0f);
  }
  if (this->temperature_high_sensor_ != nullptr) {
    this->temperature_high_sensor_->publish_state(((float) line->thigh) / 1000.0f);
  }
  if (this->voltage_low_sensor_ != nullptr) {
    this->voltage_low_sensor_->publish_state(((float) line->vlow) / 1000.0f);
  }
  if (this->voltage_high_sensor_ != nullptr) {
    this->voltage_high_sensor_->publish_state(((float) line->vhigh) / 1000.0f);
  }
  if (this->coulomb_sensor_ != nullptr) {
    this->coulomb_sensor_->publish_state(line->coulomb);
  }
  if (this->mos_temperature_sensor_ != nullptr) {
    this->mos_temperature_sensor_->publish_state(((float) line->mostempr) / 1000.0f);
  }
}

void PylontechSensor::on_cell_line_read(PylontechListener::CellLineContents *line) {
  if (this->bat_num_ != line->bat_num) {
    return;
  }
  if (line->cell_num < 0 || line->cell_num >= NUM_CELLS) {
    return;
  }
  if (this->cell_voltage_sensors_[line->cell_num] != nullptr) {
    this->cell_voltage_sensors_[line->cell_num]->publish_state(((float) line->volt) / 1000.0f);
  }
  if (this->cell_temperature_sensors_[line->cell_num] != nullptr) {
    this->cell_temperature_sensors_[line->cell_num]->publish_state(((float) line->tempr) / 1000.0f);
  }
}

}  // namespace pylontech
}  // namespace esphome

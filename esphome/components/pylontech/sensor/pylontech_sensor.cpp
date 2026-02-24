#include "pylontech_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pylontech {

static const char *const TAG = "pylontech.sensor";

void PylontechSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Pylontech Sensor:");
  ESP_LOGCONFIG(TAG, "  Battery %d", this->bat_num_);
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

void PylontechSensor::on_cell_data(const PylontechListener::CellContents *c) {
  if (this->bat_num_ != c->battery_id) {
    return; 
  }

  if (c->cell_id >= 0 && c->cell_id < 15) {
    if (this->cell_voltages_[c->cell_id] != nullptr) {
      this->cell_voltages_[c->cell_id]->publish_state(c->voltage);
    }
  }
}

}  // namespace pylontech
}  // namespace esphome

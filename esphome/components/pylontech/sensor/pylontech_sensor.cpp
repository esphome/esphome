#include "pylontech_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::pylontech {

ESPHOME_LOG_TAG(TAG, "pylontech.sensor");

void PylontechSensor::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Pylontech Sensor:\n"
                " Battery %u",
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
#ifdef PYLONTECH_SENSOR_CELL_COUNT
  for (const CellSensors &c : this->cells_) {
    ESP_LOGCONFIG(TAG, "  Cell %u", c.cell);
    LOG_SENSOR("    ", "Voltage", c.voltage);
    LOG_SENSOR("    ", "Temperature", c.temperature);
  }
#endif
}

void PylontechSensor::on_line_read(PylontechListener::LineContents *line) {
  PUBLISH_STATE(this->voltage_sensor_, ((float) line->volt) / 1000.0f);
  PUBLISH_STATE(this->current_sensor_, ((float) line->curr) / 1000.0f);
  PUBLISH_STATE(this->temperature_sensor_, ((float) line->tempr) / 1000.0f);
  PUBLISH_STATE(this->temperature_low_sensor_, ((float) line->tlow) / 1000.0f);
  PUBLISH_STATE(this->temperature_high_sensor_, ((float) line->thigh) / 1000.0f);
  PUBLISH_STATE(this->voltage_low_sensor_, ((float) line->vlow) / 1000.0f);
  PUBLISH_STATE(this->voltage_high_sensor_, ((float) line->vhigh) / 1000.0f);
  PUBLISH_STATE(this->coulomb_sensor_, line->coulomb);
  PUBLISH_STATE(this->mos_temperature_sensor_, ((float) line->mostempr) / 1000.0f);
}

#ifdef PYLONTECH_SENSOR_CELL_COUNT
void PylontechSensor::on_cells_read(const CellArray &cells, cell_mask_t read) {
  for (const CellSensors &c : this->cells_) {
    if ((read & (1u << c.cell)) == 0) {
      continue;
    }
    if (c.voltage != nullptr) {
      c.voltage->publish_state(((float) cells[c.cell].volt) / 1000.0f);
    }
    if (c.temperature != nullptr) {
      c.temperature->publish_state(((float) cells[c.cell].tempr) / 100.0f);
    }
  }
}
#endif

}  // namespace esphome::pylontech

#pragma once

#include "esphome/components/sensor/sensor.h"
#include "../pylontech.h"

namespace esphome {
namespace pylontech {

class PylontechSensor : public PylontechListener {
 public:
  PylontechSensor(int8_t bat_num) { this->bat_num_ = bat_num; }
  void dump_config() override;

  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { voltage_sensor_ = voltage_sensor; }
  void set_current_sensor(sensor::Sensor *current_sensor) { current_sensor_ = current_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_temperature_low_sensor(sensor::Sensor *temperature_low_sensor) {
    temperature_low_sensor_ = temperature_low_sensor;
  }
  void set_temperature_high_sensor(sensor::Sensor *temperature_high_sensor) {
    temperature_high_sensor_ = temperature_high_sensor;
  }
  void set_voltage_low_sensor(sensor::Sensor *voltage_low_sensor) { voltage_low_sensor_ = voltage_low_sensor; }
  void set_voltage_high_sensor(sensor::Sensor *voltage_high_sensor) { voltage_high_sensor_ = voltage_high_sensor; }
  void set_coulomb_sensor(sensor::Sensor *coulomb_sensor) { coulomb_sensor_ = coulomb_sensor; }
  void set_mos_temperature_sensor(sensor::Sensor *mos_temperature_sensor) {
    mos_temperature_sensor_ = mos_temperature_sensor;
  }

  void set_cell_voltage_sensor(int cell_index, sensor::Sensor *sensor) {
    if (cell_index >= 0 && cell_index < 15) {
      this->cell_voltages_[cell_index] = sensor;
    }
  }

  void on_line_read(LineContents *line) override;
  void on_cell_data(const CellContents *c) override;

 protected:
  int8_t bat_num_;
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *temperature_low_sensor_{nullptr};
  sensor::Sensor *temperature_high_sensor_{nullptr};
  sensor::Sensor *voltage_low_sensor_{nullptr};
  sensor::Sensor *voltage_high_sensor_{nullptr};
  sensor::Sensor *coulomb_sensor_{nullptr};
  sensor::Sensor *mos_temperature_sensor_{nullptr};

  sensor::Sensor *cell_voltages_[15]{nullptr};
};

}  // namespace pylontech
}  // namespace esphome

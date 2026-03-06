#pragma once

#include "../pylontech.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace pylontech {

class PylontechSensor : public PylontechListener, public Component {
 public:
  PylontechSensor(int8_t bat_num);
  void dump_config() override;

  SUB_SENSOR(voltage)
  SUB_SENSOR(current)
  SUB_SENSOR(temperature)
  SUB_SENSOR(temperature_low)
  SUB_SENSOR(temperature_high)
  SUB_SENSOR(voltage_low)
  SUB_SENSOR(voltage_high)

  SUB_SENSOR(coulomb)
  SUB_SENSOR(mos_temperature)

  void set_cell_voltage_sensor(int cell, sensor::Sensor *s) { this->cell_voltage_sensors_[cell] = s; }
  void set_cell_temperature_sensor(int cell, sensor::Sensor *s) { this->cell_temperature_sensors_[cell] = s; }

  void on_line_read(LineContents *line) override;
  void on_cell_line_read(CellLineContents *line) override;

 protected:
  int8_t bat_num_;
  sensor::Sensor *cell_voltage_sensors_[NUM_CELLS] = {};
  sensor::Sensor *cell_temperature_sensors_[NUM_CELLS] = {};
};

}  // namespace pylontech
}  // namespace esphome

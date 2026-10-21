#pragma once

#include "../pylontech.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::pylontech {

class PylontechSensor final : public PylontechListener {
 public:
  explicit PylontechSensor(uint8_t bat_num) : PylontechListener(bat_num) {}
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

  void on_line_read(LineContents *line) override;

#ifdef PYLONTECH_SENSOR_CELL_COUNT
  void add_cell(uint8_t cell, sensor::Sensor *voltage, sensor::Sensor *temperature) {
    this->cells_.push_back({voltage, temperature, cell});
  }
  void on_cells_read(const CellArray &cells, cell_mask_t read) override;

 protected:
  struct CellSensors {
    sensor::Sensor *voltage;
    sensor::Sensor *temperature;
    uint8_t cell;
  };
  StaticVector<CellSensors, PYLONTECH_SENSOR_CELL_COUNT> cells_;
#endif
};

}  // namespace esphome::pylontech

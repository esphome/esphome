#pragma once

#include "esphome/components/sensor/sensor.h"
#include "../pylontech.h"

namespace esphome {
namespace pylontech {

// --- NEW: Lightweight, memory-optimized sensor for individual cells ---
class PylontechCellSensor : public sensor::Sensor, public PylontechListener {
 public:
  PylontechCellSensor(int8_t bat_num, int8_t cell_id);
  void dump_config() override;
  void on_cell_data(const CellContents *c) override;
  
 protected:
  int8_t bat_num_;
  int8_t cell_id_;
};

// --- Original main sensor (without the rigid 15-cell array) ---
class PylontechSensor : public PylontechListener {
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

  void on_line_read(LineContents *line) override;

 protected:
  int8_t bat_num_;
};

}  // namespace pylontech
}  // namespace esphome

#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#include "esphome/components/modbus/modbus.h"

#include <vector>

namespace esphome {
namespace daly_hkms_bms {

static const uint8_t DALY_MODBUS_MAX_CELL_COUNT = 48;

class DalyHkmsBmsComponent : public PollingComponent, public modbus::ModbusDevice {
 public:
  void loop() override;
  void update() override;
  void on_modbus_data(const std::vector<uint8_t> &data) override;
  void dump_config() override;

  void set_daly_address(uint8_t address);

#ifdef USE_SENSOR
  void set_cell_voltage_sensor(size_t cell, sensor::Sensor *sensor) { 
    if(cell > this->cell_voltage_sensors_max_)
      this->cell_voltage_sensors_max_ = cell;
    this->cell_voltage_sensors_[cell-1] = sensor; 
  };

  SUB_SENSOR(voltage)
  SUB_SENSOR(current)
  SUB_SENSOR(battery_level)
  SUB_SENSOR(max_cell_voltage)
  SUB_SENSOR(max_cell_voltage_number)
  SUB_SENSOR(min_cell_voltage)
  SUB_SENSOR(min_cell_voltage_number)
  SUB_SENSOR(max_temperature)
  SUB_SENSOR(max_temperature_probe_number)
  SUB_SENSOR(min_temperature)
  SUB_SENSOR(min_temperature_probe_number)
  SUB_SENSOR(remaining_capacity)
  SUB_SENSOR(cycles)
  SUB_SENSOR(cells_number)
  SUB_SENSOR(temps_number)
  SUB_SENSOR(temperature_1)
  SUB_SENSOR(temperature_2)
  SUB_SENSOR(temperature_3)
  SUB_SENSOR(temperature_4)
  SUB_SENSOR(temperature_5)
  SUB_SENSOR(temperature_6)
  SUB_SENSOR(temperature_7)
  SUB_SENSOR(temperature_8)
  SUB_SENSOR(temperature_mos)
  SUB_SENSOR(temperature_board)
#endif

#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(status)
#endif

#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(charging_mos_enabled)
  SUB_BINARY_SENSOR(discharging_mos_enabled)
  SUB_BINARY_SENSOR(precharging_mos_enabled)
  SUB_BINARY_SENSOR(balancing_active)
#endif

 protected:
  bool waiting_to_update_;
  uint32_t last_send_;
  uint8_t daly_address_;

  sensor::Sensor *cell_voltage_sensors_[DALY_MODBUS_MAX_CELL_COUNT]{};
  size_t cell_voltage_sensors_max_{0};

  void advance_read_state();

  enum class ReadState{ READ_CELL_VOLTAGES, READ_DATA, IDLE } read_state_{ReadState::IDLE};
};

}  // namespace daly_hkms_bms
}  // namespace esphome

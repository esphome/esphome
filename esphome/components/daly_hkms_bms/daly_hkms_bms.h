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

class DalyHkmsBmsComponent : public PollingComponent, public modbus::ModbusDevice {
 public:
  void loop() override;
  void update() override;
  void on_modbus_data(const std::vector<uint8_t> &data) override;
  void dump_config() override;

  void set_daly_address(uint8_t address);

#ifdef USE_SENSOR
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
  SUB_SENSOR(cell_1_voltage)
  SUB_SENSOR(cell_2_voltage)
  SUB_SENSOR(cell_3_voltage)
  SUB_SENSOR(cell_4_voltage)
  SUB_SENSOR(cell_5_voltage)
  SUB_SENSOR(cell_6_voltage)
  SUB_SENSOR(cell_7_voltage)
  SUB_SENSOR(cell_8_voltage)
  SUB_SENSOR(cell_9_voltage)
  SUB_SENSOR(cell_10_voltage)
  SUB_SENSOR(cell_11_voltage)
  SUB_SENSOR(cell_12_voltage)
  SUB_SENSOR(cell_13_voltage)
  SUB_SENSOR(cell_14_voltage)
  SUB_SENSOR(cell_15_voltage)
  SUB_SENSOR(cell_16_voltage)
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

};

}  // namespace daly_hkms_bms
}  // namespace esphome

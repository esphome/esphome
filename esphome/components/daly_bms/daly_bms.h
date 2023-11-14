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
#include "esphome/components/uart/uart.h"

#include <vector>

namespace esphome {
namespace daly_bms {

class DalyBmsComponent : public PollingComponent, public uart::UARTDevice {
 public:
  DalyBmsComponent() = default;

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
  SUB_SENSOR(cells_number)
  SUB_SENSOR(temperature_1)
  SUB_SENSOR(temperature_2)
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
#endif

  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  float get_setup_priority() const override;
  void set_address(uint8_t address) { this->addr_ = address; }

 protected:
  void request_data_(uint8_t data_id);
  void decode_data_(std::vector<uint8_t> data);

  uint8_t addr_;

  std::vector<uint8_t> data_;
  bool receiving_{false};
  uint8_t data_count_;
  uint32_t last_transmission_{0};
  bool trigger_next_;
  uint8_t next_request_;
};

}  // namespace daly_bms
}  // namespace esphome

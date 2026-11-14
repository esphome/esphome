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

namespace esphome::daly_bms {

class DalyBmsComponent final : public PollingComponent, public uart::UARTDevice {
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
  SUB_SENSOR(cell_17_voltage)
  SUB_SENSOR(cell_18_voltage)
#endif

#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(status)
#endif

#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(charging_mos_enabled)
  SUB_BINARY_SENSOR(discharging_mos_enabled)
#endif

  void dump_config() override;
  void update() override;
  void loop() override;

  void set_address(uint8_t address) { this->addr_ = address; }

 protected:
  void request_data_(uint8_t data_id);
  // all frames of the response to the last request are in
  bool response_complete_() const;
  void decode_data_(std::vector<uint8_t> data);

  uint8_t addr_;

  std::vector<uint8_t> data_;
  bool receiving_{false};
  uint8_t data_count_{0};
  uint32_t last_transmission_{0};
  bool trigger_next_{false};
  // update() asked for a request cycle; it starts once no cycle is running. The first one runs at boot.
  bool cycle_requested_{true};
  // a cycle is running until the response to its last request is in or has timed out
  bool cycle_running_{false};
  // valid frames answering the last request
  uint8_t frames_received_{0};
  // frames of a cell voltage response, from the cell count in the status response; 0 until it is known
  uint8_t cell_frames_{0};
  // frames of a temperature response, seven probes per frame
  uint8_t temperature_frames_{1};
  uint8_t last_request_{0};
  // index of the next request of the cycle
  uint8_t next_request_{0};
};

}  // namespace esphome::daly_bms

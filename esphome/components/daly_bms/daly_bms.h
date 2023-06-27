#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/uart/uart.h"

#include <vector>

namespace esphome {
namespace daly_bms {

class DalyBmsComponent : public PollingComponent, public uart::UARTDevice {
 public:
  DalyBmsComponent() = default;

  // SENSORS
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { voltage_sensor_ = voltage_sensor; }
  void set_current_sensor(sensor::Sensor *current_sensor) { current_sensor_ = current_sensor; }
  void set_battery_level_sensor(sensor::Sensor *battery_level_sensor) { battery_level_sensor_ = battery_level_sensor; }
  void set_max_cell_voltage_sensor(sensor::Sensor *max_cell_voltage) { max_cell_voltage_ = max_cell_voltage; }
  void set_max_cell_voltage_number_sensor(sensor::Sensor *max_cell_voltage_number) {
    max_cell_voltage_number_ = max_cell_voltage_number;
  }
  void set_min_cell_voltage_sensor(sensor::Sensor *min_cell_voltage) { min_cell_voltage_ = min_cell_voltage; }
  void set_min_cell_voltage_number_sensor(sensor::Sensor *min_cell_voltage_number) {
    min_cell_voltage_number_ = min_cell_voltage_number;
  }
  void set_max_temperature_sensor(sensor::Sensor *max_temperature) { max_temperature_ = max_temperature; }
  void set_max_temperature_probe_number_sensor(sensor::Sensor *max_temperature_probe_number) {
    max_temperature_probe_number_ = max_temperature_probe_number;
  }
  void set_min_temperature_sensor(sensor::Sensor *min_temperature) { min_temperature_ = min_temperature; }
  void set_min_temperature_probe_number_sensor(sensor::Sensor *min_temperature_probe_number) {
    min_temperature_probe_number_ = min_temperature_probe_number;
  }
  void set_remaining_capacity_sensor(sensor::Sensor *remaining_capacity) { remaining_capacity_ = remaining_capacity; }
  void set_cells_number_sensor(sensor::Sensor *cells_number) { cells_number_ = cells_number; }
  void set_temperature_1_sensor(sensor::Sensor *temperature_1_sensor) { temperature_1_sensor_ = temperature_1_sensor; }
  void set_temperature_2_sensor(sensor::Sensor *temperature_2_sensor) { temperature_2_sensor_ = temperature_2_sensor; }
  void set_cell_1_voltage_sensor(sensor::Sensor *cell_1_voltage) { cell_1_voltage_ = cell_1_voltage; }
  void set_cell_2_voltage_sensor(sensor::Sensor *cell_2_voltage) { cell_2_voltage_ = cell_2_voltage; }
  void set_cell_3_voltage_sensor(sensor::Sensor *cell_3_voltage) { cell_3_voltage_ = cell_3_voltage; }
  void set_cell_4_voltage_sensor(sensor::Sensor *cell_4_voltage) { cell_4_voltage_ = cell_4_voltage; }
  void set_cell_5_voltage_sensor(sensor::Sensor *cell_5_voltage) { cell_5_voltage_ = cell_5_voltage; }
  void set_cell_6_voltage_sensor(sensor::Sensor *cell_6_voltage) { cell_6_voltage_ = cell_6_voltage; }
  void set_cell_7_voltage_sensor(sensor::Sensor *cell_7_voltage) { cell_7_voltage_ = cell_7_voltage; }
  void set_cell_8_voltage_sensor(sensor::Sensor *cell_8_voltage) { cell_8_voltage_ = cell_8_voltage; }
  void set_cell_9_voltage_sensor(sensor::Sensor *cell_9_voltage) { cell_9_voltage_ = cell_9_voltage; }
  void set_cell_10_voltage_sensor(sensor::Sensor *cell_10_voltage) { cell_10_voltage_ = cell_10_voltage; }
  void set_cell_11_voltage_sensor(sensor::Sensor *cell_11_voltage) { cell_11_voltage_ = cell_11_voltage; }
  void set_cell_12_voltage_sensor(sensor::Sensor *cell_12_voltage) { cell_12_voltage_ = cell_12_voltage; }
  void set_cell_13_voltage_sensor(sensor::Sensor *cell_13_voltage) { cell_13_voltage_ = cell_13_voltage; }
  void set_cell_14_voltage_sensor(sensor::Sensor *cell_14_voltage) { cell_14_voltage_ = cell_14_voltage; }
  void set_cell_15_voltage_sensor(sensor::Sensor *cell_15_voltage) { cell_15_voltage_ = cell_15_voltage; }
  void set_cell_16_voltage_sensor(sensor::Sensor *cell_16_voltage) { cell_16_voltage_ = cell_16_voltage; }

  // TEXT_SENSORS
  void set_status_text_sensor(text_sensor::TextSensor *status_text_sensor) { status_text_sensor_ = status_text_sensor; }
  // BINARY_SENSORS
  void set_charging_mos_enabled_binary_sensor(binary_sensor::BinarySensor *charging_mos_enabled) {
    charging_mos_enabled_ = charging_mos_enabled;
  }
  void set_discharging_mos_enabled_binary_sensor(binary_sensor::BinarySensor *discharging_mos_enabled) {
    discharging_mos_enabled_ = discharging_mos_enabled;
  }

  void setup() override;
  void dump_config() override;
  void update() override;

  float get_setup_priority() const override;
  void set_address(uint8_t address) { this->addr_ = address; }

 protected:
  void request_data_(uint8_t data_id);
  void decode_data_(std::vector<uint8_t> data);

  uint8_t addr_;

  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *battery_level_sensor_{nullptr};
  sensor::Sensor *max_cell_voltage_{nullptr};
  sensor::Sensor *max_cell_voltage_number_{nullptr};
  sensor::Sensor *min_cell_voltage_{nullptr};
  sensor::Sensor *min_cell_voltage_number_{nullptr};
  sensor::Sensor *max_temperature_{nullptr};
  sensor::Sensor *max_temperature_probe_number_{nullptr};
  sensor::Sensor *min_temperature_{nullptr};
  sensor::Sensor *min_temperature_probe_number_{nullptr};
  sensor::Sensor *remaining_capacity_{nullptr};
  sensor::Sensor *cells_number_{nullptr};
  sensor::Sensor *temperature_1_sensor_{nullptr};
  sensor::Sensor *temperature_2_sensor_{nullptr};
  sensor::Sensor *cell_1_voltage_{nullptr};
  sensor::Sensor *cell_2_voltage_{nullptr};
  sensor::Sensor *cell_3_voltage_{nullptr};
  sensor::Sensor *cell_4_voltage_{nullptr};
  sensor::Sensor *cell_5_voltage_{nullptr};
  sensor::Sensor *cell_6_voltage_{nullptr};
  sensor::Sensor *cell_7_voltage_{nullptr};
  sensor::Sensor *cell_8_voltage_{nullptr};
  sensor::Sensor *cell_9_voltage_{nullptr};
  sensor::Sensor *cell_10_voltage_{nullptr};
  sensor::Sensor *cell_11_voltage_{nullptr};
  sensor::Sensor *cell_12_voltage_{nullptr};
  sensor::Sensor *cell_13_voltage_{nullptr};
  sensor::Sensor *cell_14_voltage_{nullptr};
  sensor::Sensor *cell_15_voltage_{nullptr};
  sensor::Sensor *cell_16_voltage_{nullptr};

  text_sensor::TextSensor *status_text_sensor_{nullptr};

  binary_sensor::BinarySensor *charging_mos_enabled_{nullptr};
  binary_sensor::BinarySensor *discharging_mos_enabled_{nullptr};
};

}  // namespace daly_bms
}  // namespace esphome

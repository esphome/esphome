#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/uart/uart.h"

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

 protected:
  void request_data_(uint8_t data_id);
  void decode_data_(std::vector<uint8_t> data);

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

  text_sensor::TextSensor *status_text_sensor_{nullptr};

  binary_sensor::BinarySensor *charging_mos_enabled_{nullptr};
  binary_sensor::BinarySensor *discharging_mos_enabled_{nullptr};
};

}  // namespace daly_bms
}  // namespace esphome

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"

#include <vector>

namespace esphome {
namespace chargery_bms {

class ChargeryBmsComponent : public uart::UARTDevice, public Component {
 public:
  ChargeryBmsComponent() = default;

  // SENSORS
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { voltage_sensor_ = voltage_sensor; }
  void set_charge_end_voltage_sensor(sensor::Sensor *charge_end_voltage_sensor) { 
	  charge_end_voltage_sensor_ = charge_end_voltage_sensor;
  }
  void set_current_sensor(sensor::Sensor *current_sensor) { current_sensor_ = current_sensor; }
  void set_current1_sensor(sensor::Sensor *current1_sensor) { current1_sensor_ = current1_sensor; }
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
  void set_remaining_capacity_ah_sensor(sensor::Sensor *remaining_capacity_ah) { 
	  remaining_capacity_ah_ = remaining_capacity_ah;
  }
  void set_remaining_capacity_wh_sensor(sensor::Sensor *remaining_capacity_wh) { 
	  remaining_capacity_wh_ = remaining_capacity_wh;
  }
  void set_cells_number_sensor(sensor::Sensor *cells_number) { cells_number_ = cells_number; }
  void set_temperature_1_sensor(sensor::Sensor *temperature_1_sensor) { temperature_1_sensor_ = temperature_1_sensor; }
  void set_temperature_2_sensor(sensor::Sensor *temperature_2_sensor) { temperature_2_sensor_ = temperature_2_sensor; }
  void set_cell_1_voltage_sensor(sensor::Sensor *cell_1_voltage) { cell_voltages_[0] = cell_1_voltage; }
  void set_cell_2_voltage_sensor(sensor::Sensor *cell_2_voltage) { cell_voltages_[1] = cell_2_voltage; }
  void set_cell_3_voltage_sensor(sensor::Sensor *cell_3_voltage) { cell_voltages_[2] = cell_3_voltage; }
  void set_cell_4_voltage_sensor(sensor::Sensor *cell_4_voltage) { cell_voltages_[3] = cell_4_voltage; }
  void set_cell_5_voltage_sensor(sensor::Sensor *cell_5_voltage) { cell_voltages_[4] = cell_5_voltage; }
  void set_cell_6_voltage_sensor(sensor::Sensor *cell_6_voltage) { cell_voltages_[5] = cell_6_voltage; }
  void set_cell_7_voltage_sensor(sensor::Sensor *cell_7_voltage) { cell_voltages_[6] = cell_7_voltage; }
  void set_cell_8_voltage_sensor(sensor::Sensor *cell_8_voltage) { cell_voltages_[7] = cell_8_voltage; }
  void set_cell_9_voltage_sensor(sensor::Sensor *cell_9_voltage) { cell_voltages_[8] = cell_9_voltage; }
  void set_cell_10_voltage_sensor(sensor::Sensor *cell_10_voltage) { cell_voltages_[9] = cell_10_voltage; }
  void set_cell_11_voltage_sensor(sensor::Sensor *cell_11_voltage) { cell_voltages_[10] = cell_11_voltage; }
  void set_cell_12_voltage_sensor(sensor::Sensor *cell_12_voltage) { cell_voltages_[11] = cell_12_voltage; }
  void set_cell_13_voltage_sensor(sensor::Sensor *cell_13_voltage) { cell_voltages_[12] = cell_13_voltage; }
  void set_cell_14_voltage_sensor(sensor::Sensor *cell_14_voltage) { cell_voltages_[13] = cell_14_voltage; }
  void set_cell_15_voltage_sensor(sensor::Sensor *cell_15_voltage) { cell_voltages_[14] = cell_15_voltage; }
  void set_cell_16_voltage_sensor(sensor::Sensor *cell_16_voltage) { cell_voltages_[15] = cell_16_voltage; }
  void set_cell_17_voltage_sensor(sensor::Sensor *cell_17_voltage) { cell_voltages_[16] = cell_17_voltage; }
  void set_cell_18_voltage_sensor(sensor::Sensor *cell_18_voltage) { cell_voltages_[17] = cell_18_voltage; }
  void set_cell_19_voltage_sensor(sensor::Sensor *cell_19_voltage) { cell_voltages_[18] = cell_19_voltage; }
  void set_cell_20_voltage_sensor(sensor::Sensor *cell_20_voltage) { cell_voltages_[19] = cell_20_voltage; }
  void set_cell_21_voltage_sensor(sensor::Sensor *cell_21_voltage) { cell_voltages_[20] = cell_21_voltage; }
  void set_cell_22_voltage_sensor(sensor::Sensor *cell_22_voltage) { cell_voltages_[21] = cell_22_voltage; }
  void set_cell_23_voltage_sensor(sensor::Sensor *cell_23_voltage) { cell_voltages_[22] = cell_23_voltage; }
  void set_cell_24_voltage_sensor(sensor::Sensor *cell_24_voltage) { cell_voltages_[23] = cell_24_voltage; }
  void set_cell_1_impedance_sensor(sensor::Sensor *cell_1_impedance) { cell_impedances_[0] = cell_1_impedance; }
  void set_cell_2_impedance_sensor(sensor::Sensor *cell_2_impedance) { cell_impedances_[1] = cell_2_impedance; }
  void set_cell_3_impedance_sensor(sensor::Sensor *cell_3_impedance) { cell_impedances_[2] = cell_3_impedance; }
  void set_cell_4_impedance_sensor(sensor::Sensor *cell_4_impedance) { cell_impedances_[3] = cell_4_impedance; }
  void set_cell_5_impedance_sensor(sensor::Sensor *cell_5_impedance) { cell_impedances_[4] = cell_5_impedance; }
  void set_cell_6_impedance_sensor(sensor::Sensor *cell_6_impedance) { cell_impedances_[5] = cell_6_impedance; }
  void set_cell_7_impedance_sensor(sensor::Sensor *cell_7_impedance) { cell_impedances_[6] = cell_7_impedance; }
  void set_cell_8_impedance_sensor(sensor::Sensor *cell_8_impedance) { cell_impedances_[7] = cell_8_impedance; }
  void set_cell_9_impedance_sensor(sensor::Sensor *cell_9_impedance) { cell_impedances_[8] = cell_9_impedance; }
  void set_cell_10_impedance_sensor(sensor::Sensor *cell_10_impedance) { cell_impedances_[9] = cell_10_impedance; }
  void set_cell_11_impedance_sensor(sensor::Sensor *cell_11_impedance) { cell_impedances_[10] = cell_11_impedance; }
  void set_cell_12_impedance_sensor(sensor::Sensor *cell_12_impedance) { cell_impedances_[11] = cell_12_impedance; }
  void set_cell_13_impedance_sensor(sensor::Sensor *cell_13_impedance) { cell_impedances_[12] = cell_13_impedance; }
  void set_cell_14_impedance_sensor(sensor::Sensor *cell_14_impedance) { cell_impedances_[13] = cell_14_impedance; }
  void set_cell_15_impedance_sensor(sensor::Sensor *cell_15_impedance) { cell_impedances_[14] = cell_15_impedance; }
  void set_cell_16_impedance_sensor(sensor::Sensor *cell_16_impedance) { cell_impedances_[15] = cell_16_impedance; }
  void set_cell_17_impedance_sensor(sensor::Sensor *cell_17_impedance) { cell_impedances_[16] = cell_17_impedance; }
  void set_cell_18_impedance_sensor(sensor::Sensor *cell_18_impedance) { cell_impedances_[17] = cell_18_impedance; }
  void set_cell_19_impedance_sensor(sensor::Sensor *cell_19_impedance) { cell_impedances_[18] = cell_19_impedance; }
  void set_cell_20_impedance_sensor(sensor::Sensor *cell_20_impedance) { cell_impedances_[19] = cell_20_impedance; }
  void set_cell_21_impedance_sensor(sensor::Sensor *cell_21_impedance) { cell_impedances_[20] = cell_21_impedance; }
  void set_cell_22_impedance_sensor(sensor::Sensor *cell_22_impedance) { cell_impedances_[21] = cell_22_impedance; }
  void set_cell_23_impedance_sensor(sensor::Sensor *cell_23_impedance) { cell_impedances_[22] = cell_23_impedance; }
  void set_cell_24_impedance_sensor(sensor::Sensor *cell_24_impedance) { cell_impedances_[23] = cell_24_impedance; }

  // TEXT_SENSORS
  void set_current_mode_sensor(text_sensor::TextSensor *current_mode_sensor) {
	  current_mode_sensor_ = current_mode_sensor;
  }
  void set_current1_mode_sensor(text_sensor::TextSensor *current1_mode_sensor) {
	  current1_mode_sensor_ = current1_mode_sensor;
  }

  void setup() override;
  void dump_config() override;
  void update();
  void loop() override;

  float get_setup_priority() const override;
  void set_battery_num_cells(uint8_t cells) { battery_num_cells_ = cells; }

 protected:
  uint8_t battery_num_cells_ = 24;
  std::vector<uint8_t> packet_;
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *charge_end_voltage_sensor_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *current1_sensor_{nullptr};
  sensor::Sensor *battery_level_sensor_{nullptr};
  sensor::Sensor *max_cell_voltage_{nullptr};
  sensor::Sensor *max_cell_voltage_number_{nullptr};
  sensor::Sensor *min_cell_voltage_{nullptr};
  sensor::Sensor *min_cell_voltage_number_{nullptr};
  sensor::Sensor *max_cell_impedance_{nullptr};
  sensor::Sensor *max_cell_impedance_number_{nullptr};
  sensor::Sensor *min_cell_impedance_{nullptr};
  sensor::Sensor *min_cell_impedance_number_{nullptr};
  sensor::Sensor *max_temperature_{nullptr};
  sensor::Sensor *max_temperature_probe_number_{nullptr};
  sensor::Sensor *min_temperature_{nullptr};
  sensor::Sensor *min_temperature_probe_number_{nullptr};
  sensor::Sensor *remaining_capacity_ah_{nullptr};
  sensor::Sensor *remaining_capacity_wh_{nullptr};
  sensor::Sensor *cells_number_{nullptr};
  sensor::Sensor *temperature_1_sensor_{nullptr};
  sensor::Sensor *temperature_2_sensor_{nullptr};
  std::vector<sensor::Sensor *> cell_voltages_ = {
	  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  };
  std::vector<sensor::Sensor *> cell_impedances_ = {
	  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  };

  text_sensor::TextSensor *current_mode_sensor_{nullptr};
  text_sensor::TextSensor *current1_mode_sensor_{nullptr};

  void read_packet_();
  void get_in_sync_();
  void decode_packet_();
  void packet_consumed_(uint8_t len);
  void decode_status_cells_(uint8_t cells);
  void decode_status_bms_();
  void decode_status_impedances_(uint8_t cells);
  void skip_malformed_packet_();
};

}  // namespace chargery_bms
}  // namespace esphome

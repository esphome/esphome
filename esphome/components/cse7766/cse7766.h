#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace cse7766 {

class CSE7766Component : public Component, public uart::UARTDevice {
 public:
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { voltage_sensor_ = voltage_sensor; }
  void set_current_sensor(sensor::Sensor *current_sensor) { current_sensor_ = current_sensor; }
  void set_power_sensor(sensor::Sensor *power_sensor) { power_sensor_ = power_sensor; }
  void set_energy_sensor(sensor::Sensor *energy_sensor) { energy_sensor_ = energy_sensor; }
  void set_apparent_power_sensor(sensor::Sensor *apparent_power_sensor) {
    apparent_power_sensor_ = apparent_power_sensor;
  }
  void set_power_factor_sensor(sensor::Sensor *power_factor_sensor) { power_factor_sensor_ = power_factor_sensor; }

  void loop() override;
  float get_setup_priority() const override;
  void dump_config() override;

 protected:
  bool check_byte_();
  void parse_data_();
  uint32_t get_24_bit_uint_(uint8_t start_index);

  uint8_t raw_data_[24];
  uint8_t raw_data_index_{0};
  uint32_t last_transmission_{0};
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};
  sensor::Sensor *energy_sensor_{nullptr};
  sensor::Sensor *apparent_power_sensor_{nullptr};
  sensor::Sensor *power_factor_sensor_{nullptr};
  uint32_t cf_pulses_total_{0};
  uint16_t cf_pulses_last_{0};
};

}  // namespace cse7766
}  // namespace esphome

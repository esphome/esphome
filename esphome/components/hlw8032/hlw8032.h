#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

#include <cinttypes>

namespace esphome {
namespace hlw8032 {

class HLW8032Component : public Component, public uart::UARTDevice {
 public:
  void loop() override;
  void dump_config() override;

  void set_current_resistor(float current_resistor) { current_resistor_ = current_resistor; }
  void set_voltage_divider(float voltage_divider) { voltage_divider_ = voltage_divider; }
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { voltage_sensor_ = voltage_sensor; }
  void set_current_sensor(sensor::Sensor *current_sensor) { current_sensor_ = current_sensor; }
  void set_power_sensor(sensor::Sensor *power_sensor) { power_sensor_ = power_sensor; }
  void set_apparent_power_sensor(sensor::Sensor *apparent_power_sensor) {
    apparent_power_sensor_ = apparent_power_sensor;
  }
  void set_power_factor_sensor(sensor::Sensor *power_factor_sensor) { power_factor_sensor_ = power_factor_sensor; }

 protected:
  void parse_data_();
  uint32_t get_24_bit_uint_(uint8_t start_index);

  bool header_found_{false};
  uint8_t check_{0};
  uint8_t raw_data_[24]{};
  uint8_t raw_data_index_{0};
  uint32_t last_transmission_{0};
  float current_resistor_{0.001};
  float voltage_divider_{1.720};
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};
  sensor::Sensor *apparent_power_sensor_{nullptr};
  sensor::Sensor *power_factor_sensor_{nullptr};
};

}  // namespace hlw8032
}  // namespace esphome

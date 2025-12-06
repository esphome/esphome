#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome::hlw8032 {

class HLW8032Component : public Component, public uart::UARTDevice {
 public:
  void loop() override;
  void dump_config() override;

  void set_current_resistor(float current_resistor) { this->current_resistor_ = current_resistor; }
  void set_voltage_divider(float voltage_divider) { this->voltage_divider_ = voltage_divider; }
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { this->voltage_sensor_ = voltage_sensor; }
  void set_current_sensor(sensor::Sensor *current_sensor) { this->current_sensor_ = current_sensor; }
  void set_power_sensor(sensor::Sensor *power_sensor) { this->power_sensor_ = power_sensor; }
  void set_apparent_power_sensor(sensor::Sensor *apparent_power_sensor) {
    this->apparent_power_sensor_ = apparent_power_sensor;
  }
  void set_power_factor_sensor(sensor::Sensor *power_factor_sensor) {
    this->power_factor_sensor_ = power_factor_sensor;
  }

 protected:
  void parse_data_();
  uint32_t read_uint24_(uint8_t offset);

  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};
  sensor::Sensor *apparent_power_sensor_{nullptr};
  sensor::Sensor *power_factor_sensor_{nullptr};

  float current_resistor_{0.001f};
  float voltage_divider_{1.720f};
  uint8_t raw_data_[24]{};
  uint8_t check_{0};
  uint8_t raw_data_index_{0};
  bool header_found_{false};
};

}  // namespace esphome::hlw8032

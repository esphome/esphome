#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/line_reader.h"
#include "esphome/components/uart/uart.h"

namespace esphome::smt100 {

class SMT100Component final : public PollingComponent, public uart::UARTDevice {
  // 33 characters plus the NUL; the longest line, "15000,80.00,100.00,-20.00,12.00", is 31
  static constexpr uint16_t LINE_BUFFER_SIZE = 34;

 public:
  SMT100Component() = default;

  void dump_config() override;
  void loop() override;
  void update() override;

  void set_counts_sensor(sensor::Sensor *counts_sensor) { this->counts_sensor_ = counts_sensor; }
  void set_permittivity_sensor(sensor::Sensor *permittivity_sensor) {
    this->permittivity_sensor_ = permittivity_sensor;
  }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { this->temperature_sensor_ = temperature_sensor; }
  void set_moisture_sensor(sensor::Sensor *moisture_sensor) { this->moisture_sensor_ = moisture_sensor; }
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { this->voltage_sensor_ = voltage_sensor; }

 protected:
  LineReader<LINE_BUFFER_SIZE, '\r', '\n'> line_reader_;

  sensor::Sensor *counts_sensor_{nullptr};
  sensor::Sensor *permittivity_sensor_{nullptr};
  sensor::Sensor *moisture_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *voltage_sensor_{nullptr};

  uint32_t last_transmission_{0};
};

}  // namespace esphome::smt100

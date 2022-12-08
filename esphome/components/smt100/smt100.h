#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace smt100 {

class SMT100Component : public PollingComponent, public uart::UARTDevice {
  static const uint16_t MAX_LINE_LENGTH = 31;

 public:
  SMT100Component() = default;

  void dump_config() override;
  void loop() override;
  void update() override;

  float get_setup_priority() const override;

  void set_counts_sensor(sensor::Sensor *counts_sensor) { this->counts_sensor_ = counts_sensor; }
  void set_dielectric_constant_sensor(sensor::Sensor *dielectric_constant_sensor) {
    this->dielectric_constant_sensor_ = dielectric_constant_sensor;
  }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { this->temperature_sensor_ = temperature_sensor; }
  void set_moisture_sensor(sensor::Sensor *moisture_sensor) { this->moisture_sensor_ = moisture_sensor; }
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { this->voltage_sensor_ = voltage_sensor; }

 protected:
  int readline_(int readch, char *buffer, int len);

  sensor::Sensor *counts_sensor_{nullptr};
  sensor::Sensor *dielectric_constant_sensor_{nullptr};
  sensor::Sensor *moisture_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *voltage_sensor_{nullptr};

  uint32_t last_transmission_{0};
};

}  // namespace smt100
}  // namespace esphome

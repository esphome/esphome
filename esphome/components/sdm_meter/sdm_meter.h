#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/modbus/modbus.h"

#include <vector>

namespace esphome {
namespace sdm_meter {

class SDMMeter : public PollingComponent, public modbus::ModbusDevice {
 public:
  void set_voltage_sensor(uint8_t phase, sensor::Sensor *voltage_sensor) {
    this->phases_[phase].setup = true;
    this->phases_[phase].voltage_sensor_ = voltage_sensor;
  }
  void set_current_sensor(uint8_t phase, sensor::Sensor *current_sensor) {
    this->phases_[phase].setup = true;
    this->phases_[phase].current_sensor_ = current_sensor;
  }
  void set_active_power_sensor(uint8_t phase, sensor::Sensor *active_power_sensor) {
    this->phases_[phase].setup = true;
    this->phases_[phase].active_power_sensor_ = active_power_sensor;
  }
  void set_apparent_power_sensor(uint8_t phase, sensor::Sensor *apparent_power_sensor) {
    this->phases_[phase].setup = true;
    this->phases_[phase].apparent_power_sensor_ = apparent_power_sensor;
  }
  void set_reactive_power_sensor(uint8_t phase, sensor::Sensor *reactive_power_sensor) {
    this->phases_[phase].setup = true;
    this->phases_[phase].reactive_power_sensor_ = reactive_power_sensor;
  }
  void set_power_factor_sensor(uint8_t phase, sensor::Sensor *power_factor_sensor) {
    this->phases_[phase].setup = true;
    this->phases_[phase].power_factor_sensor_ = power_factor_sensor;
  }
  void set_phase_angle_sensor(uint8_t phase, sensor::Sensor *phase_angle_sensor) {
    this->phases_[phase].setup = true;
    this->phases_[phase].phase_angle_sensor_ = phase_angle_sensor;
  }
  void set_total_power_sensor(sensor::Sensor *total_power_sensor) { this->total_power_sensor_ = total_power_sensor; }
  void set_frequency_sensor(sensor::Sensor *frequency_sensor) { this->frequency_sensor_ = frequency_sensor; }
  void set_import_active_energy_sensor(sensor::Sensor *import_active_energy_sensor) {
    this->import_active_energy_sensor_ = import_active_energy_sensor;
  }
  void set_export_active_energy_sensor(sensor::Sensor *export_active_energy_sensor) {
    this->export_active_energy_sensor_ = export_active_energy_sensor;
  }
  void set_import_reactive_energy_sensor(sensor::Sensor *import_reactive_energy_sensor) {
    this->import_reactive_energy_sensor_ = import_reactive_energy_sensor;
  }
  void set_export_reactive_energy_sensor(sensor::Sensor *export_reactive_energy_sensor) {
    this->export_reactive_energy_sensor_ = export_reactive_energy_sensor;
  }

  void update() override;

  void on_modbus_data(const std::vector<uint8_t> &data) override;

  void dump_config() override;

 protected:
  struct SDMPhase {
    bool setup{false};
    sensor::Sensor *voltage_sensor_{nullptr};
    sensor::Sensor *current_sensor_{nullptr};
    sensor::Sensor *active_power_sensor_{nullptr};
    sensor::Sensor *apparent_power_sensor_{nullptr};
    sensor::Sensor *reactive_power_sensor_{nullptr};
    sensor::Sensor *power_factor_sensor_{nullptr};
    sensor::Sensor *phase_angle_sensor_{nullptr};
  } phases_[3];
  sensor::Sensor *frequency_sensor_{nullptr};
  sensor::Sensor *total_power_sensor_{nullptr};
  sensor::Sensor *import_active_energy_sensor_{nullptr};
  sensor::Sensor *export_active_energy_sensor_{nullptr};
  sensor::Sensor *import_reactive_energy_sensor_{nullptr};
  sensor::Sensor *export_reactive_energy_sensor_{nullptr};
};

}  // namespace sdm_meter
}  // namespace esphome

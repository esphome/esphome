#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/modbus/modbus.h"

namespace esphome {
namespace sdm220m {

class SDM220M : public PollingComponent, public modbus::ModbusDevice {
 public:
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { voltage_sensor_ = voltage_sensor; }
  void set_current_sensor(sensor::Sensor *current_sensor) { current_sensor_ = current_sensor; }
  void set_active_power_sensor(sensor::Sensor *active_power_sensor) { active_power_sensor_ = active_power_sensor; }
  void set_apparent_power_sensor(sensor::Sensor *apparent_power_sensor) {
    apparent_power_sensor_ = apparent_power_sensor;
  }
  void set_reactive_power_sensor(sensor::Sensor *reactive_power_sensor) {
    reactive_power_sensor_ = reactive_power_sensor;
  }
  void set_power_factor_sensor(sensor::Sensor *power_factor_sensor) { power_factor_sensor_ = power_factor_sensor; }
  void set_phase_angle_sensor(sensor::Sensor *phase_angle_sensor) { phase_angle_sensor_ = phase_angle_sensor; }
  void set_frequency_sensor(sensor::Sensor *frequency_sensor) { frequency_sensor_ = frequency_sensor; }
  void set_import_active_energy_sensor(sensor::Sensor *import_active_energy_sensor) {
    import_active_energy_sensor_ = import_active_energy_sensor;
  }
  void set_export_active_energy_sensor(sensor::Sensor *export_active_energy_sensor) {
    export_active_energy_sensor_ = export_active_energy_sensor;
  }
  void set_import_reactive_energy_sensor(sensor::Sensor *import_reactive_energy_sensor) {
    import_reactive_energy_sensor_ = import_reactive_energy_sensor;
  }
  void set_export_reactive_energy_sensor(sensor::Sensor *export_reactive_energy_sensor) {
    export_reactive_energy_sensor_ = export_reactive_energy_sensor;
  }

  void update() override;

  void on_modbus_data(const std::vector<uint8_t> &data) override;

  void dump_config() override;

 protected:
  sensor::Sensor *voltage_sensor_;
  sensor::Sensor *current_sensor_;
  sensor::Sensor *active_power_sensor_;
  sensor::Sensor *apparent_power_sensor_;
  sensor::Sensor *reactive_power_sensor_;
  sensor::Sensor *power_factor_sensor_;
  sensor::Sensor *phase_angle_sensor_;
  sensor::Sensor *frequency_sensor_;
  sensor::Sensor *import_active_energy_sensor_;
  sensor::Sensor *export_active_energy_sensor_;
  sensor::Sensor *import_reactive_energy_sensor_;
  sensor::Sensor *export_reactive_energy_sensor_;
};

}  // namespace sdm220m
}  // namespace esphome

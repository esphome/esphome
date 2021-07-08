#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/modbus/modbus.h"

namespace esphome {
namespace selec_meter {

class SelecMeter : public PollingComponent, public modbus::ModbusDevice {
 public:
  void set_total_active_energy_sensor(sensor::Sensor *total_active_energy_sensor) {
    this->total_active_energy_sensor_ = total_active_energy_sensor;
  }
  void set_import_active_energy_sensor(sensor::Sensor *import_active_energy_sensor) {
    this->import_active_energy_sensor_ = import_active_energy_sensor;
  }
  void set_export_active_energy_sensor(sensor::Sensor *export_active_energy_sensor) {
    this->export_active_energy_sensor_ = export_active_energy_sensor;
  }
  void set_total_reactive_energy_sensor(sensor::Sensor *total_reactive_energy_sensor) {
    this->total_reactive_energy_sensor_ = total_reactive_energy_sensor;
  }
  void set_import_reactive_energy_sensor(sensor::Sensor *import_reactive_energy_sensor) {
    this->import_reactive_energy_sensor_ = import_reactive_energy_sensor;
  }
  void set_export_reactive_energy_sensor(sensor::Sensor *export_reactive_energy_sensor) {
    this->export_reactive_energy_sensor_ = export_reactive_energy_sensor;
  }
  void set_apparent_energy_sensor(sensor::Sensor *apparent_energy_sensor) {
    this->apparent_energy_sensor_ = apparent_energy_sensor;
  }
  void set_active_power_sensor(sensor::Sensor *active_power_sensor) {
    this->active_power_sensor_ = active_power_sensor;
  }
  void set_reactive_power_sensor(sensor::Sensor *reactive_power_sensor) {
    this->reactive_power_sensor_ = reactive_power_sensor;
  }
  void set_apparent_power_sensor(sensor::Sensor *apparent_power_sensor) {
    this->apparent_power_sensor_ = apparent_power_sensor;
  }
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) {
    this->voltage_sensor_ = voltage_sensor;
  }
  void set_current_sensor(sensor::Sensor *current_sensor) {
    this->current_sensor_ = current_sensor;
  }
  void set_power_factor_sensor(sensor::Sensor *power_factor_sensor) {
    this->power_factor_sensor_ = power_factor_sensor;
  }
  void set_frequency_sensor(sensor::Sensor *frequency_sensor) { this->frequency_sensor_ = frequency_sensor; }
  void set_maximum_demand_active_power_sensor(sensor::Sensor *maximum_demand_active_power_sensor) {
    this->maximum_demand_active_power_sensor_ = maximum_demand_active_power_sensor;
  }
  void set_maximum_demand_reactive_power_sensor(sensor::Sensor *maximum_demand_reactive_power_sensor) {
    this->maximum_demand_reactive_power_sensor_ = maximum_demand_reactive_power_sensor;
  }
  void set_maximum_demand_apparent_power_sensor(sensor::Sensor *maximum_demand_apparent_power_sensor) {
    this->maximum_demand_apparent_power_sensor_ = maximum_demand_apparent_power_sensor;
  }

  void update() override;

  void on_modbus_data(const std::vector<uint8_t> &data) override;

  void dump_config() override;

 protected:
  sensor::Sensor *total_active_energy_sensor_{nullptr};
  sensor::Sensor *import_active_energy_sensor_{nullptr};
  sensor::Sensor *export_active_energy_sensor_{nullptr};
  sensor::Sensor *total_reactive_energy_sensor_{nullptr};
  sensor::Sensor *import_reactive_energy_sensor_{nullptr};
  sensor::Sensor *export_reactive_energy_sensor_{nullptr};
  sensor::Sensor *apparent_energy_sensor_{nullptr};
  sensor::Sensor *active_power_sensor_{nullptr};
  sensor::Sensor *reactive_power_sensor_{nullptr};
  sensor::Sensor *apparent_power_sensor_{nullptr};
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *power_factor_sensor_{nullptr};
  sensor::Sensor *frequency_sensor_{nullptr};
  sensor::Sensor *maximum_demand_active_power_sensor_{nullptr};
  sensor::Sensor *maximum_demand_reactive_power_sensor_{nullptr};
  sensor::Sensor *maximum_demand_apparent_power_sensor_{nullptr};
};

}  // namespace selec_meter
}  // namespace esphome

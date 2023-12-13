#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/modbus/modbus.h"

#include <vector>

namespace esphome {
namespace havells_solar {

class HavellsSolar : public PollingComponent, public modbus::ModbusDevice {
 public:
  void set_voltage_sensor(uint8_t phase, sensor::Sensor *voltage_sensor) {
    this->phases_[phase].setup = true;
    this->phases_[phase].voltage_sensor_ = voltage_sensor;
  }
  void set_current_sensor(uint8_t phase, sensor::Sensor *current_sensor) {
    this->phases_[phase].setup = true;
    this->phases_[phase].current_sensor_ = current_sensor;
  }
  void set_voltage_sensor_pv(uint8_t pv, sensor::Sensor *voltage_sensor) {
    this->pvs_[pv].setup = true;
    this->pvs_[pv].voltage_sensor_ = voltage_sensor;
  }
  void set_current_sensor_pv(uint8_t pv, sensor::Sensor *current_sensor) {
    this->pvs_[pv].setup = true;
    this->pvs_[pv].current_sensor_ = current_sensor;
  }
  void set_active_power_sensor_pv(uint8_t pv, sensor::Sensor *active_power_sensor) {
    this->pvs_[pv].setup = true;
    this->pvs_[pv].active_power_sensor_ = active_power_sensor;
  }
  void set_voltage_sampled_by_secondary_cpu_sensor_pv(uint8_t pv,
                                                      sensor::Sensor *voltage_sampled_by_secondary_cpu_sensor) {
    this->pvs_[pv].setup = true;
    this->pvs_[pv].voltage_sampled_by_secondary_cpu_sensor_ = voltage_sampled_by_secondary_cpu_sensor;
  }
  void set_insulation_of_p_to_ground_sensor_pv(uint8_t pv, sensor::Sensor *insulation_of_p_to_ground_sensor) {
    this->pvs_[pv].setup = true;
    this->pvs_[pv].insulation_of_p_to_ground_sensor_ = insulation_of_p_to_ground_sensor;
  }
  void set_frequency_sensor(sensor::Sensor *frequency_sensor) { this->frequency_sensor_ = frequency_sensor; }
  void set_active_power_sensor(sensor::Sensor *active_power_sensor) {
    this->active_power_sensor_ = active_power_sensor;
  }
  void set_reactive_power_sensor(sensor::Sensor *reactive_power_sensor) {
    this->reactive_power_sensor_ = reactive_power_sensor;
  }
  void set_today_production_sensor(sensor::Sensor *today_production_sensor) {
    this->today_production_sensor_ = today_production_sensor;
  }
  void set_total_energy_production_sensor(sensor::Sensor *total_energy_production_sensor) {
    this->total_energy_production_sensor_ = total_energy_production_sensor;
  }
  void set_total_generation_time_sensor(sensor::Sensor *total_generation_time_sensor) {
    this->total_generation_time_sensor_ = total_generation_time_sensor;
  }
  void set_today_generation_time_sensor(sensor::Sensor *today_generation_time_sensor) {
    this->today_generation_time_sensor_ = today_generation_time_sensor;
  }
  void set_inverter_module_temp_sensor(sensor::Sensor *inverter_module_temp_sensor) {
    this->inverter_module_temp_sensor_ = inverter_module_temp_sensor;
  }
  void set_inverter_inner_temp_sensor(sensor::Sensor *inverter_inner_temp_sensor) {
    this->inverter_inner_temp_sensor_ = inverter_inner_temp_sensor;
  }
  void set_inverter_bus_voltage_sensor(sensor::Sensor *inverter_bus_voltage_sensor) {
    this->inverter_bus_voltage_sensor_ = inverter_bus_voltage_sensor;
  }
  void set_insulation_pv_n_to_ground_sensor(sensor::Sensor *insulation_pv_n_to_ground_sensor) {
    this->insulation_pv_n_to_ground_sensor_ = insulation_pv_n_to_ground_sensor;
  }
  void set_gfci_value_sensor(sensor::Sensor *gfci_value_sensor) { this->gfci_value_sensor_ = gfci_value_sensor; }
  void set_dci_of_r_sensor(sensor::Sensor *dci_of_r_sensor) { this->dci_of_r_sensor_ = dci_of_r_sensor; }
  void set_dci_of_s_sensor(sensor::Sensor *dci_of_s_sensor) { this->dci_of_s_sensor_ = dci_of_s_sensor; }
  void set_dci_of_t_sensor(sensor::Sensor *dci_of_t_sensor) { this->dci_of_t_sensor_ = dci_of_t_sensor; }

  void update() override;

  void on_modbus_data(const std::vector<uint8_t> &data) override;

  void dump_config() override;

 protected:
  struct HAVELLSPhase {
    bool setup{false};
    sensor::Sensor *voltage_sensor_{nullptr};
    sensor::Sensor *current_sensor_{nullptr};
  } phases_[3];
  struct HAVELLSPV {
    bool setup{false};
    sensor::Sensor *voltage_sensor_{nullptr};
    sensor::Sensor *current_sensor_{nullptr};
    sensor::Sensor *active_power_sensor_{nullptr};
    sensor::Sensor *voltage_sampled_by_secondary_cpu_sensor_{nullptr};
    sensor::Sensor *insulation_of_p_to_ground_sensor_{nullptr};
  } pvs_[2];
  sensor::Sensor *frequency_sensor_{nullptr};
  sensor::Sensor *active_power_sensor_{nullptr};
  sensor::Sensor *reactive_power_sensor_{nullptr};
  sensor::Sensor *today_production_sensor_{nullptr};
  sensor::Sensor *total_energy_production_sensor_{nullptr};
  sensor::Sensor *total_generation_time_sensor_{nullptr};
  sensor::Sensor *today_generation_time_sensor_{nullptr};
  sensor::Sensor *inverter_module_temp_sensor_{nullptr};
  sensor::Sensor *inverter_inner_temp_sensor_{nullptr};
  sensor::Sensor *inverter_bus_voltage_sensor_{nullptr};
  sensor::Sensor *insulation_pv_n_to_ground_sensor_{nullptr};
  sensor::Sensor *gfci_value_sensor_{nullptr};
  sensor::Sensor *dci_of_r_sensor_{nullptr};
  sensor::Sensor *dci_of_s_sensor_{nullptr};
  sensor::Sensor *dci_of_t_sensor_{nullptr};
};

}  // namespace havells_solar
}  // namespace esphome

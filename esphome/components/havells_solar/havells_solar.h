#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/modbus/modbus.h"

#include <span>

namespace esphome::havells_solar {

class HavellsSolar final : public PollingComponent, public modbus::ModbusClientDevice {
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
  SUB_SENSOR(frequency)
  SUB_SENSOR(active_power)
  SUB_SENSOR(reactive_power)
  SUB_SENSOR(today_production)
  SUB_SENSOR(total_energy_production)
  SUB_SENSOR(total_generation_time)
  SUB_SENSOR(today_generation_time)
  SUB_SENSOR(inverter_module_temp)
  SUB_SENSOR(inverter_inner_temp)
  SUB_SENSOR(inverter_bus_voltage)
  SUB_SENSOR(insulation_pv_n_to_ground)
  SUB_SENSOR(gfci_value)
  SUB_SENSOR(dci_of_r)
  SUB_SENSOR(dci_of_s)
  SUB_SENSOR(dci_of_t)

  void update() override;

  void on_read_holding_registers(uint16_t start_address, std::span<const uint16_t> registers,
                                 modbus::ResponseStatus status) override;

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
};

}  // namespace esphome::havells_solar

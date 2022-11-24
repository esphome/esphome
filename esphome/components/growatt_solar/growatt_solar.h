#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/modbus/modbus.h"

#include <vector>

namespace esphome {
namespace growatt_solar {

static const float TWO_DEC_UNIT = 0.01;
static const float ONE_DEC_UNIT = 0.1;

enum GrowattProtocolVersion {
  RTU = 0,
  RTU2,
};

class GrowattSolar : public PollingComponent, public modbus::ModbusDevice {
 public:
  void update() override;
  void on_modbus_data(const std::vector<uint8_t> &data) override;
  void dump_config() override;

  void set_protocol_version(GrowattProtocolVersion protocol_version) { this->protocol_version_ = protocol_version; }

  void set_inverter_status_sensor(sensor::Sensor *sensor) { this->inverter_status_ = sensor; }

  void set_grid_frequency_sensor(sensor::Sensor *sensor) { this->grid_frequency_sensor_ = sensor; }
  void set_grid_active_power_sensor(sensor::Sensor *sensor) { this->grid_active_power_sensor_ = sensor; }
  void set_pv_active_power_sensor(sensor::Sensor *sensor) { this->pv_active_power_sensor_ = sensor; }

  void set_today_production_sensor(sensor::Sensor *sensor) { this->today_production_ = sensor; }
  void set_total_energy_production_sensor(sensor::Sensor *sensor) { this->total_energy_production_ = sensor; }
  void set_inverter_module_temp_sensor(sensor::Sensor *sensor) { this->inverter_module_temp_ = sensor; }

  void set_voltage_sensor(uint8_t phase, sensor::Sensor *voltage_sensor) {
    this->phases_[phase].voltage_sensor_ = voltage_sensor;
  }
  void set_current_sensor(uint8_t phase, sensor::Sensor *current_sensor) {
    this->phases_[phase].current_sensor_ = current_sensor;
  }
  void set_active_power_sensor(uint8_t phase, sensor::Sensor *active_power_sensor) {
    this->phases_[phase].active_power_sensor_ = active_power_sensor;
  }
  void set_voltage_sensor_pv(uint8_t pv, sensor::Sensor *voltage_sensor) {
    this->pvs_[pv].voltage_sensor_ = voltage_sensor;
  }
  void set_current_sensor_pv(uint8_t pv, sensor::Sensor *current_sensor) {
    this->pvs_[pv].current_sensor_ = current_sensor;
  }
  void set_active_power_sensor_pv(uint8_t pv, sensor::Sensor *active_power_sensor) {
    this->pvs_[pv].active_power_sensor_ = active_power_sensor;
  }

 protected:
  struct GrowattPhase {
    sensor::Sensor *voltage_sensor_{nullptr};
    sensor::Sensor *current_sensor_{nullptr};
    sensor::Sensor *active_power_sensor_{nullptr};
  } phases_[3];
  struct GrowattPV {
    sensor::Sensor *voltage_sensor_{nullptr};
    sensor::Sensor *current_sensor_{nullptr};
    sensor::Sensor *active_power_sensor_{nullptr};
  } pvs_[2];

  sensor::Sensor *inverter_status_{nullptr};

  sensor::Sensor *grid_frequency_sensor_{nullptr};
  sensor::Sensor *grid_active_power_sensor_{nullptr};

  sensor::Sensor *pv_active_power_sensor_{nullptr};

  sensor::Sensor *today_production_{nullptr};
  sensor::Sensor *total_energy_production_{nullptr};
  sensor::Sensor *inverter_module_temp_{nullptr};
  GrowattProtocolVersion protocol_version_;
};

}  // namespace growatt_solar
}  // namespace esphome

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/modbus/modbus.h"

#include <vector>

namespace esphome::growatt_solar {

static const float TWO_DEC_UNIT = 0.01;
static const float ONE_DEC_UNIT = 0.1;

enum GrowattProtocolVersion {
  RTU = 0,
  RTU2,
};

// Register addresses for the RTU protocol.
constexpr size_t RTU_INVERTER_STATUS = 0;           // length = 1
constexpr size_t RTU_PV_ACTIVE_POWER = 1;           // length = 2
constexpr size_t RTU_PV1_VOLTAGE = 3;               // length = 1
constexpr size_t RTU_PV1_CURRENT = 4;               // length = 1
constexpr size_t RTU_PV1_ACTIVE_POWER = 5;          // length = 2
constexpr size_t RTU_PV2_VOLTAGE = 7;               // length = 1
constexpr size_t RTU_PV2_CURRENT = 8;               // length = 1
constexpr size_t RTU_PV2_ACTIVE_POWER = 9;          // length = 2
constexpr size_t RTU_GRID_ACTIVE_POWER = 11;        // length = 2
constexpr size_t RTU_GRID_FREQUENCY = 13;           // length = 1
constexpr size_t RTU_PHASE1_VOLTAGE = 14;           // length = 1
constexpr size_t RTU_PHASE1_CURRENT = 15;           // length = 1
constexpr size_t RTU_PHASE1_ACTIVE_POWER = 16;      // length = 2
constexpr size_t RTU_PHASE2_VOLTAGE = 18;           // length = 1
constexpr size_t RTU_PHASE2_CURRENT = 19;           // length = 1
constexpr size_t RTU_PHASE2_ACTIVE_POWER = 20;      // length = 2
constexpr size_t RTU_PHASE3_VOLTAGE = 22;           // length = 1
constexpr size_t RTU_PHASE3_CURRENT = 23;           // length = 1
constexpr size_t RTU_PHASE3_ACTIVE_POWER = 24;      // length = 2
constexpr size_t RTU_TODAY_PRODUCTION = 26;         // length = 2
constexpr size_t RTU_TOTAL_ENERGY_PRODUCTION = 28;  // length = 2
constexpr size_t RTU_INVERTER_MODULE_TEMP = 32;     // length = 1

// Input register addresses for the RTU2 protocol as described
// in the "GROWATT INVERTER MODBUS PROTOCOL_II V1.39" document.
constexpr size_t RTU2_INVERTER_STATUS = 0;           // length = 1
constexpr size_t RTU2_PV_ACTIVE_POWER = 1;           // length = 2
constexpr size_t RTU2_PV1_VOLTAGE = 3;               // length = 1
constexpr size_t RTU2_PV1_CURRENT = 4;               // length = 1
constexpr size_t RTU2_PV1_ACTIVE_POWER = 5;          // length = 2
constexpr size_t RTU2_PV2_VOLTAGE = 7;               // length = 1
constexpr size_t RTU2_PV2_CURRENT = 8;               // length = 1
constexpr size_t RTU2_PV2_ACTIVE_POWER = 9;          // length = 2
constexpr size_t RTU2_GRID_ACTIVE_POWER = 35;        // length = 2
constexpr size_t RTU2_GRID_FREQUENCY = 37;           // length = 1
constexpr size_t RTU2_PHASE1_VOLTAGE = 38;           // length = 1
constexpr size_t RTU2_PHASE1_CURRENT = 39;           // length = 1
constexpr size_t RTU2_PHASE1_ACTIVE_POWER = 40;      // length = 2
constexpr size_t RTU2_PHASE2_VOLTAGE = 42;           // length = 1
constexpr size_t RTU2_PHASE2_CURRENT = 43;           // length = 1
constexpr size_t RTU2_PHASE2_ACTIVE_POWER = 44;      // length = 2
constexpr size_t RTU2_PHASE3_VOLTAGE = 46;           // length = 1
constexpr size_t RTU2_PHASE3_CURRENT = 47;           // length = 1
constexpr size_t RTU2_PHASE3_ACTIVE_POWER = 48;      // length = 2
constexpr size_t RTU2_TODAY_PRODUCTION = 53;         // length = 2
constexpr size_t RTU2_TOTAL_ENERGY_PRODUCTION = 55;  // length = 2
constexpr size_t RTU2_INVERTER_MODULE_TEMP = 93;     // length = 1

class GrowattSolar final : public PollingComponent, public modbus::ModbusClientDevice {
 public:
  void loop() override;
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
  bool waiting_to_update_{false};
  uint32_t last_send_{0};

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

}  // namespace esphome::growatt_solar

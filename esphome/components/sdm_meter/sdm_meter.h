#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/modbus/modbus.h"

#include <span>

namespace esphome::sdm_meter {

class SDMMeter final : public PollingComponent, public modbus::ModbusClientDevice {
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
  SUB_SENSOR(total_power)
  SUB_SENSOR(frequency)
  SUB_SENSOR(import_active_energy)
  SUB_SENSOR(export_active_energy)
  SUB_SENSOR(import_reactive_energy)
  SUB_SENSOR(export_reactive_energy)

  void update() override;

  void on_read_input_registers(uint16_t start_address, std::span<const uint16_t> registers,
                               modbus::ResponseStatus status) override;

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
};

}  // namespace esphome::sdm_meter

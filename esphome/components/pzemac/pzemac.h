#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/modbus/modbus.h"

namespace esphome {
namespace pzemac {

class PZEMAC : public PollingComponent, public modbus::ModbusDevice {
 public:
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { voltage_sensor_ = voltage_sensor; }
  void set_current_sensor(sensor::Sensor *current_sensor) { current_sensor_ = current_sensor; }
  void set_power_sensor(sensor::Sensor *power_sensor) { power_sensor_ = power_sensor; }
  void set_energy_sensor(sensor::Sensor *energy_sensor) { energy_sensor_ = energy_sensor; }
  void set_frequency_sensor(sensor::Sensor *frequency_sensor) { frequency_sensor_ = frequency_sensor; }
  void set_power_factor_sensor(sensor::Sensor *power_factor_sensor) { power_factor_sensor_ = power_factor_sensor; }

  void set_update_filter(uint8_t update_filter) {
    update_filter_ = update_filter;
    update_ok_count_down_ = update_filter;
    update_not_ok_count_down_ = update_filter;
  }

  void update() override;

  void on_modbus_data(const std::vector<uint8_t> &data) override;

  void dump_config() override;

 protected:
  sensor::Sensor *voltage_sensor_;
  sensor::Sensor *current_sensor_;
  sensor::Sensor *power_sensor_;
  sensor::Sensor *energy_sensor_;
  sensor::Sensor *frequency_sensor_;
  sensor::Sensor *power_factor_sensor_;
  bool modbus_has_data_{false};
  uint8_t update_filter_{0};
  uint8_t update_ok_count_down_{0};
  uint8_t update_not_ok_count_down_{0};
};

}  // namespace pzemac
}  // namespace esphome

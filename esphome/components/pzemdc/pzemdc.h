#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/modbus/modbus.h"

#include <vector>

namespace esphome {
namespace pzemdc {

class PZEMDC : public PollingComponent, public modbus::ModbusDevice {
 public:
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { voltage_sensor_ = voltage_sensor; }
  void set_current_sensor(sensor::Sensor *current_sensor) { current_sensor_ = current_sensor; }
  void set_power_sensor(sensor::Sensor *power_sensor) { power_sensor_ = power_sensor; }
  void set_energy_sensor(sensor::Sensor *energy_sensor) { energy_sensor_ = energy_sensor; }

  void update() override;

  void on_modbus_data(const std::vector<uint8_t> &data) override;

  void dump_config() override;

  void reset_energy();

 protected:
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};
  sensor::Sensor *energy_sensor_{nullptr};
};

template<typename... Ts> class ResetEnergyAction : public Action<Ts...> {
 public:
  ResetEnergyAction(PZEMDC *pzemdc) : pzemdc_(pzemdc) {}

  void play(Ts... x) override { this->pzemdc_->reset_energy(); }

 protected:
  PZEMDC *pzemdc_;
};

}  // namespace pzemdc
}  // namespace esphome

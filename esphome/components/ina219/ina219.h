#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

#include <cinttypes>

namespace esphome::ina219 {

class INA219Component final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(bus_voltage)
  SUB_SENSOR(shunt_voltage)
  SUB_SENSOR(current)
  SUB_SENSOR(power)

 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  void on_powerdown() override;

  void set_shunt_resistance_ohm(float shunt_resistance_ohm) { shunt_resistance_ohm_ = shunt_resistance_ohm; }
  void set_max_current_a(float max_current_a) { max_current_a_ = max_current_a; }
  void set_max_voltage_v(float max_voltage_v) { max_voltage_v_ = max_voltage_v; }

 protected:
  float shunt_resistance_ohm_;
  float max_current_a_;
  float max_voltage_v_;
  uint32_t calibration_lsb_;
};

}  // namespace esphome::ina219

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace ms5611 {

class MS5611Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { pressure_sensor_ = pressure_sensor; }

 protected:
  void read_temperature_();
  void read_pressure_(uint32_t raw_temperature);
  void calculate_values_(uint32_t raw_temperature, uint32_t raw_pressure);

  sensor::Sensor *temperature_sensor_;
  sensor::Sensor *pressure_sensor_;
  uint16_t prom_[6];
};

}  // namespace ms5611
}  // namespace esphome

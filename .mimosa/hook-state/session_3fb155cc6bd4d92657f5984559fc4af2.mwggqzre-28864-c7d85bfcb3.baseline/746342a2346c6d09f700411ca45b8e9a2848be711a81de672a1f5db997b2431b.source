#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::ms5611 {

class MS5611Component final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(temperature)
  SUB_SENSOR(pressure)

 public:
  void setup() override;
  void dump_config() override;
  void update() override;

 protected:
  void read_temperature_();
  void read_pressure_(uint32_t raw_temperature);
  void calculate_values_(uint32_t raw_temperature, uint32_t raw_pressure);

  uint16_t prom_[6];
};

}  // namespace esphome::ms5611

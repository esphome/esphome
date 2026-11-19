#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::lps22 {

class LPS22Component final : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(temperature)
  SUB_SENSOR(pressure)

 public:
  void setup() override;
  void update() override;
  void dump_config() override;

 protected:
  void try_read_();

  uint8_t read_attempts_remaining_{0};
};

}  // namespace esphome::lps22

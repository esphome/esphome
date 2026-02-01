#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::stts22h {

class STTS22HComponent : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

 protected:
  void initialize_sensor_();
  bool is_stts22h_sensor_();
  float read_temperature_();
};

}  // namespace esphome::stts22h

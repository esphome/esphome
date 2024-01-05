#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome {
namespace cdm7160 {

class CDM7160Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override;

  void set_co2_sensor(sensor::Sensor *co2_sensor) { co2_sensor_ = co2_sensor; }

  void set_altitude(float altitude) { altitude_ = altitude; }

 protected:
  sensor::Sensor *co2_sensor_{nullptr};
  bool write_altitude_();
  
  float altitude_ = 5.0f;
};
}  // namespace cdm7160
}  // namespace esphome

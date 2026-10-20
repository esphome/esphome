#pragma once

#include <limits>
#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::gy39v3 {

class GY39v3 final : public i2c::I2CDevice, public sensor::Sensor, public PollingComponent {
 public:
  static const char TAG[];
  SUB_SENSOR(illuminance)
  SUB_SENSOR(temperature)
  SUB_SENSOR(pressure)
  SUB_SENSOR(humidity)
  SUB_SENSOR(altitude)
  SUB_SENSOR(vapor_pressure_deficit)
  void set_delta(int baz) { this->delta_ = baz; }

  void setup() override;
  void update() override;
  void dump_config() override;

 private:
  float delta_{std::numeric_limits<float>::quiet_NaN()};
  float calculate_vpd(float t, float rh);
};

}  // namespace esphome::gy39v3

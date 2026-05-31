#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome::hte501 {

/// This class implements support for the hte501 of temperature i2c sensors.
class HTE501Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }

  void setup() override;
  void dump_config() override;
  void update() override;

 protected:
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};

  enum ErrorCode { NONE = 0, COMMUNICATION_FAILED, CRC_CHECK_FAILED } error_code_{NONE};
};

}  // namespace esphome::hte501

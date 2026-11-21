#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::opt3001 {

/// This class implements support for the i2c-based OPT3001 ambient light sensor.
class OPT3001Sensor final : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void dump_config() override;
  void update() override;

 protected:
  // checks if one-shot is complete, then reads and publishes the result
  void read_result_(uint8_t retries_left);
  // ends the update: publishes the reading, or NAN on failure
  void publish_lx_(float val);

  bool updating_{false};
};

}  // namespace esphome::opt3001

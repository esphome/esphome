#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace opt3001 {

/// This class implements support for the i2c-based OPT3001 ambient light sensor.
class OPT3001Sensor : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override;

 protected:
  // checks if one-shot is complete before reading the result and returning it
  void read_result_(const std::function<void(float)> &f);
  // begins a one-shot measurement
  void read_lx_(const std::function<void(float)> &f);

  bool updating_{false};
};

}  // namespace opt3001
}  // namespace esphome

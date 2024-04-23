#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace opt3001 {

/// This class implements support for the i2c-based OPT3001 ambient light sensor.
class OPT3001Sensor : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  OPT3001Sensor();

  void dump_config() override;
  void update() override;
  float get_setup_priority() const override;

 protected:
  // checks if one-shot is complete before reading the result and returnig it
  void read_result_(const std::function<void(float)> &f);
  // begins a one-shot measurement
  void read_lx_(const std::function<void(float)> &f);
  // begins a read, but doesn't actually do it
  i2c::ErrorCode setup_read_(uint8_t a_register) { return this->write(&a_register, 1, true); }
  // reads without setting the register first
  i2c::ErrorCode read_(uint16_t *data) { return this->read(reinterpret_cast<uint8_t *>(data), 2); }

  bool updating_;
};

}  // namespace opt3001
}  // namespace esphome

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

static const unsigned char CRC8_ONEWIRE_POLY = 0x31;
static const unsigned char CRC8_ONEWIRE_START = 0xFF;

namespace esphome {
namespace hte501 {

/// This class implements support for the hte501 of temperature i2c sensors.
class HTE501Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }

  float get_setup_priority() const override;
  void setup() override;
  void dump_config() override;
  void update() override;

 protected:
  unsigned char calcCrc8(unsigned char buf[], unsigned char from, unsigned char to);
  sensor::Sensor *temperature_sensor_;
  sensor::Sensor *humidity_sensor_;

  enum ErrorCode { NONE = 0, COMMUNICATION_FAILED, CRC_CHECK_FAILED } error_code_{NONE};
 };

}  // namespace hte501
}  // namespace esphome

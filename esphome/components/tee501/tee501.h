#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

static const unisgned char CRC8_ONEWIRE_POLY 0x31;
static const unisgned char CRC8_ONEWIRE_START 0xFF;

namespace esphome {
namespace tee501 {

/// This class implements support for the tee501 of temperature i2c sensors.
class TEE501Component : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

  protected:
  unsigned char calcCrc8 (unsigned char buf[], unsigned char from, unsigned char to);

  enum ErrorCode {
  NONE = 0,
  COMMUNICATION_FAILED,
  CRC_CHECK_FAILED
  } error_code_{NONE};
};

}  // namespace tee501
}  // namespace esphome

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace lm75b {

static const uint8_t LM75B_REG_TEMPERATURE = 0x00;

class LM75BComponent : public PollingComponent, public i2c::I2CDevice, public sensor::Sensor {
 public:
  void dump_config() override;
  void update() override;
};

}  // namespace lm75b
}  // namespace esphome

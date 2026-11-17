#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome::husb238 {

static constexpr uint8_t REG_PD_STATUS1 = 0x01;
static constexpr uint8_t PD_STATUS1_ATTACHED = 1 << 6;
static constexpr uint8_t PD_STATUS1_CC_DIR = 1 << 7;

class Husb238Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(attached)
  SUB_BINARY_SENSOR(cc_direction)
#endif
};

}  // namespace esphome::husb238

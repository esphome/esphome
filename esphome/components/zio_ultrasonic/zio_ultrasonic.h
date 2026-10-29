#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/log.h"

ESPHOME_LOG_TAG(TAG, "Zio Ultrasonic");

namespace esphome::zio_ultrasonic {

class ZioUltrasonicComponent final : public i2c::I2CDevice, public PollingComponent, public sensor::Sensor {
 public:
  void dump_config() override;

  void update() override;
};

}  // namespace esphome::zio_ultrasonic

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

static const char *const TAG = "Zio Ultrasonic";

namespace esphome {
namespace zio_ultrasonic {

class ZioUltrasonicComponent : public i2c::I2CDevice, public PollingComponent, public sensor::Sensor {
 public:
  float get_setup_priority() const override { return setup_priority::DATA; }

  void dump_config() override;

  void update() override;
};

}  // namespace zio_ultrasonic
}  // namespace esphome

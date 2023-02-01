#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensirion_common/i2c_sensirion.h"

namespace esphome {
namespace sts3x {

/// This class implements support for the ST3x-DIS family of temperature i2c sensors.
class STS3XComponent : public sensor::Sensor, public PollingComponent, public sensirion_common::SensirionI2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;
};

}  // namespace sts3x
}  // namespace esphome

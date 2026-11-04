#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensirion_common/i2c_sensirion.h"

namespace esphome::sfa30 {

class SFA30Component final : public PollingComponent, public sensirion_common::SensirionI2CDevice {
  SUB_SENSOR(formaldehyde)
  SUB_SENSOR(humidity)
  SUB_SENSOR(temperature)

 public:
  void setup() override;
  void dump_config() override;
  void update() override;

 protected:
  char device_marking_[32] = {0};
};

}  // namespace esphome::sfa30

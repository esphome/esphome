#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensirion_common/i2c_sensirion.h"

namespace esphome::shtcx {

enum SHTCXType : uint8_t {
  SHTCX_TYPE_SHTC3 = 0,
  SHTCX_TYPE_SHTC1,
  SHTCX_TYPE_UNKNOWN,
};

/// This class implements support for the SHT3x-DIS family of temperature+humidity i2c sensors.
class SHTCXComponent final : public PollingComponent, public sensirion_common::SensirionI2CDevice {
  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)

 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  void soft_reset();
  void sleep();
  void wake_up();

 protected:
  uint16_t sensor_id_;
  SHTCXType type_;
};

}  // namespace esphome::shtcx

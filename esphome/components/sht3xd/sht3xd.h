#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensirion_common/i2c_sensirion.h"

namespace esphome::sht3xd {

/// This class implements support for the SHT3x-DIS family of temperature+humidity i2c sensors.
class SHT3XDComponent final : public PollingComponent, public sensirion_common::SensirionI2CDevice {
  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)

 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  void set_heater_enabled(bool heater_enabled) { heater_enabled_ = heater_enabled; }

 protected:
  bool heater_enabled_{true};
  uint32_t serial_number_{0};
};

}  // namespace esphome::sht3xd

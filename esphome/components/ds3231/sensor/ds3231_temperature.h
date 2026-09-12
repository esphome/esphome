#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"

#include "../ds3231.h"

namespace esphome::ds3231 {

class DS3231TemperatureSensor final : public sensor::Sensor, public PollingComponent, public Parented<DS3231Component> {
 public:
  void update() override;
  void dump_config() override;

  void set_fahrenheit(bool fahrenheit) { this->fahrenheit_ = fahrenheit; }

 protected:
  bool fahrenheit_{false};
};

}  // namespace esphome::ds3231

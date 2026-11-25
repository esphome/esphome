#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::dew_point {

class DewPointComponent final : public Component, public sensor::Sensor {
  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)

 public:
  void setup() override;
  void dump_config() override;
  void loop() override;

  float get_setup_priority() const override;

 protected:
  float temperature_value_{NAN};
  float humidity_value_{NAN};
};

}  // namespace esphome::dew_point

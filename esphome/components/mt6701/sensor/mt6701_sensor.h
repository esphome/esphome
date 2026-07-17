#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/mt6701/mt6701.h"

namespace esphome::mt6701 {

/// Publishes readings from an MT6701 hub (I2C or SSI/SPI) as ESPHome sensors.
///
/// The main entity is the absolute shaft angle in degrees; the raw count is an
/// optional sub-sensor. Derived values such as multi-turn position or speed are
/// left to the user to compute in YAML (see the README examples).
class MT6701Sensor final : public PollingComponent, public Parented<MT6701Component>, public sensor::Sensor {
  SUB_SENSOR(raw_count)

 public:
  void update() override;
  void dump_config() override;
};

}  // namespace esphome::mt6701

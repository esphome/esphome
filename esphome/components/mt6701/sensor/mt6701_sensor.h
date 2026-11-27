#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/mt6701/mt6701.h"

namespace esphome::mt6701 {

/// The shaft angle in degrees, with the raw count as an optional sub-sensor
class MT6701Sensor final : public PollingComponent, public Parented<MT6701Component>, public sensor::Sensor {
  SUB_SENSOR(raw_count)

 public:
  void update() override;
  void dump_config() override;
};

}  // namespace esphome::mt6701

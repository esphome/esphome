#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "../seesaw.h"

namespace esphome::seesaw {

class SeesawTemperature : public sensor::Sensor, public PollingComponent, public Parented<Seesaw> {
 public:
  void update() override;
  void dump_config() override;
};

}  // namespace esphome::seesaw

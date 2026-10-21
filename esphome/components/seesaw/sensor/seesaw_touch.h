#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "../seesaw.h"

namespace esphome::seesaw {

class SeesawTouch : public sensor::Sensor, public PollingComponent, public Parented<Seesaw> {
 public:
  explicit SeesawTouch(uint8_t channel) : channel_(channel) {}

  void update() override;
  void dump_config() override;

 protected:
  const uint8_t channel_;
};

}  // namespace esphome::seesaw

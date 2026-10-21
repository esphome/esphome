#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "../seesaw.h"

namespace esphome::seesaw {

class SeesawADC : public sensor::Sensor, public PollingComponent, public Parented<Seesaw> {
 public:
  explicit SeesawADC(uint8_t pin) : pin_(pin) {}

  void update() override;
  void dump_config() override;

 protected:
  const uint8_t pin_;
};

}  // namespace esphome::seesaw

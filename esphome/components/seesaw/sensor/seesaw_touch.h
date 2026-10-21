#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "../seesaw.h"

namespace esphome::seesaw {

class SeesawTouch : public sensor::Sensor, public PollingComponent {
 public:
  explicit SeesawTouch(uint8_t channel) : channel_(channel) {}

  void setup() override;
  void update() override;

  void set_parent(Seesaw *parent) { parent_ = parent; }

 protected:
  Seesaw *parent_;
  const uint8_t channel_;
};

}  // namespace esphome::seesaw

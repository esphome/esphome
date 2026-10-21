#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "../seesaw.h"

namespace esphome::seesaw {

class SeesawADC : public sensor::Sensor, public PollingComponent {
 public:
  explicit SeesawADC(uint8_t pin) : pin_(pin) {}

  void setup() override;
  void update() override;

  void set_parent(Seesaw *parent) { this->parent_ = parent; }

 protected:
  Seesaw *parent_;
  const uint8_t pin_;
};

}  // namespace esphome::seesaw

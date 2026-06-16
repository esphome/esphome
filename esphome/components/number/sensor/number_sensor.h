#pragma once

#include "../number.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::number {

class NumberSensor : public sensor::Sensor, public Component {
 public:
  explicit NumberSensor(Number *source) : source_(source) {}
  void setup() override;
  void dump_config() override;

 protected:
  Number *source_;
};

}  // namespace esphome::number

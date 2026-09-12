#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "../seesaw.h"

namespace esphome::seesaw {

class SeesawEncoder : public sensor::Sensor, public Component {
 public:
  void setup() override;
  void loop() override;

  void set_parent(Seesaw *parent) { parent_ = parent; }
  void set_number(uint8_t number) { number_ = number; }
  void set_min_value(int32_t min_value) { this->min_value_ = min_value; }
  void set_max_value(int32_t max_value) { this->max_value_ = max_value; }

 protected:
  Seesaw *parent_;
  uint8_t number_{0};
  int32_t value_{0};
  int32_t min_value_{INT32_MIN};
  int32_t max_value_{INT32_MAX};
};

}  // namespace esphome::seesaw

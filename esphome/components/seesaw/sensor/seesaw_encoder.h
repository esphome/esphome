#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "../seesaw.h"

namespace esphome::seesaw {

class SeesawEncoder : public sensor::Sensor, public Component, public Parented<Seesaw> {
 public:
  explicit SeesawEncoder(uint8_t number) : number_(number) {}

  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_min_value(int32_t min_value) { this->min_value_ = min_value; }
  void set_max_value(int32_t max_value) { this->max_value_ = max_value; }

 protected:
  const uint8_t number_;
  int32_t value_{0};
  int32_t last_position_{0};
  uint32_t last_poll_{0};
  int32_t min_value_{INT32_MIN};
  int32_t max_value_{INT32_MAX};
};

}  // namespace esphome::seesaw

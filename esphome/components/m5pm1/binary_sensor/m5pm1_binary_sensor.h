#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"

#include "../m5pm1.h"

namespace esphome::m5pm1 {

class M5PM1PowerButtonBinarySensor : public binary_sensor::BinarySensor, public Component {
 public:
  explicit M5PM1PowerButtonBinarySensor(M5PM1Component *parent) : parent_(parent) {}

  void setup() override;
  void loop() override;
  void dump_config() override;

 protected:
  void update_state_();

  M5PM1Component *const parent_;
  uint32_t last_poll_ms_{0};
};

}  // namespace esphome::m5pm1

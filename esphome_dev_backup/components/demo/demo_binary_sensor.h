#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace demo {

class DemoBinarySensor : public binary_sensor::BinarySensor, public PollingComponent {
 public:
  void setup() override { this->publish_initial_state(false); }
  void update() override {
    bool new_state = last_state_ = !last_state_;
    this->publish_state(new_state);
  }

 protected:
  bool last_state_ = false;
};

}  // namespace demo
}  // namespace esphome

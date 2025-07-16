#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace demo {

class DemoSensor : public sensor::Sensor, public PollingComponent {
 public:
  void update() override {
    float val = random_float();
    bool increasing = this->get_state_class() == sensor::STATE_CLASS_TOTAL_INCREASING;
    if (increasing) {
      float base = std::isnan(this->state) ? 0.0f : this->state;
      this->publish_state(base + val * 10);
    } else {
      if (val < 0.1) {
        this->publish_state(NAN);
      } else {
        this->publish_state(val * 100);
      }
    }
  }
};

}  // namespace demo
}  // namespace esphome

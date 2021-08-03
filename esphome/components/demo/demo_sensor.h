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
    bool is_auto = this->last_reset_type == sensor::LAST_RESET_TYPE_AUTO;
    if (is_auto) {
      float base = isnan(this->state) ? 0.0f : this->state;
      this->publish_state(base + val * 10);
    } else {
      if (val < 0.1)
        this->publish_state(NAN);
      else
        this->publish_state(val * 100);
    }
  }
};

}  // namespace demo
}  // namespace esphome

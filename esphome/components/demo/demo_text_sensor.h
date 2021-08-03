#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace demo {

class DemoTextSensor : public text_sensor::TextSensor, public PollingComponent {
 public:
  void update() override {
    float val = random_float();
    if (val < 0.33) {
      this->publish_state("foo");
    } else if (val < 0.66) {
      this->publish_state("bar");
    } else {
      this->publish_state("foobar");
    }
  }
};

}  // namespace demo
}  // namespace esphome

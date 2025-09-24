#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace demo {

class DemoSwitch : public switch_::Switch, public Component {
 public:
  void setup() override {
    bool initial = random_float() < 0.5;
    this->publish_state(initial);
  }

 protected:
  void write_state(bool state) override { this->publish_state(state); }
};

}  // namespace demo
}  // namespace esphome

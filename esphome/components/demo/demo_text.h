#pragma once

#include "esphome/components/text/text.h"
#include "esphome/core/component.h"

namespace esphome::demo {

class DemoText : public text::Text, public Component {
 public:
  void setup() override { this->publish_state("I am a text entity"); }

 protected:
  void control(const std::string &value) override { this->publish_state(value); }
};

}  // namespace esphome::demo

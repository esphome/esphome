#pragma once

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace demo {

enum class DemoNumberType {
  TYPE_1,
  TYPE_2,
  TYPE_3,
};

class DemoNumber : public number::Number, public Component {
 public:
  void set_type(DemoNumberType type) { type_ = type; }
  void setup() override {
    switch (type_) {
      case DemoNumberType::TYPE_1:
        this->publish_state(50);
        break;
      case DemoNumberType::TYPE_2:
        this->publish_state(-10);
        break;
      case DemoNumberType::TYPE_3:
        this->publish_state(42);
        break;
    }
  }

 protected:
  void control(float value) override { this->publish_state(value); }

  DemoNumberType type_;
};

}  // namespace demo
}  // namespace esphome

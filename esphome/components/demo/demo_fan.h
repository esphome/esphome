#pragma once

#include "esphome/core/component.h"
#include "esphome/components/fan/fan_state.h"

namespace esphome {
namespace demo {

enum class DemoFanType {
  TYPE_1,
  TYPE_2,
  TYPE_3,
  TYPE_4,
};

class DemoFan : public Component {
 public:
  void set_type(DemoFanType type) { type_ = type; }
  void set_fan(fan::FanState *fan) { fan_ = fan; }
  void setup() override {
    fan::FanTraits traits{};

    // oscillation
    // speed
    // direction
    // speed_count
    switch (type_) {
      case DemoFanType::TYPE_1:
        break;
      case DemoFanType::TYPE_2:
        traits.set_oscillation(true);
        break;
      case DemoFanType::TYPE_3:
        traits.set_direction(true);
        traits.set_speed(true);
        traits.set_supported_speed_count(5);
        break;
      case DemoFanType::TYPE_4:
        traits.set_direction(true);
        traits.set_speed(true);
        traits.set_supported_speed_count(100);
        traits.set_oscillation(true);
        break;
    }

    this->fan_->set_traits(traits);
  }

  fan::FanState *fan_;
  DemoFanType type_;
};

}  // namespace demo
}  // namespace esphome

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/fan/fan.h"

namespace esphome::demo {

enum class DemoFanType {
  TYPE_1,
  TYPE_2,
  TYPE_3,
  TYPE_4,
};

class DemoFan : public fan::Fan, public Component {
 public:
  void set_type(DemoFanType type) { type_ = type; }
  fan::FanTraits get_traits() override {
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

    return traits;
  }

 protected:
  void control(const fan::FanCall &call) override {
    auto state = call.get_state();
    if (state.has_value())
      this->state = *state;
    auto oscillating = call.get_oscillating();
    if (oscillating.has_value())
      this->oscillating = *oscillating;
    auto speed = call.get_speed();
    if (speed.has_value())
      this->speed = *speed;
    auto direction = call.get_direction();
    if (direction.has_value())
      this->direction = *direction;

    this->publish_state();
  }

  DemoFanType type_;
};

}  // namespace esphome::demo

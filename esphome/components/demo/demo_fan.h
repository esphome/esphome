#pragma once

#include "esphome/core/component.h"
#include "esphome/components/fan/fan.h"

namespace esphome {
namespace demo {

enum class DemoFanType {
  TYPE_1,
  TYPE_2,
  TYPE_3,
  TYPE_4,
};

class DemoFan : public fan::Fan, public Component {
 public:
  void set_type(DemoFanType type) { type_ = type; }
  const fan::FanTraits &get_traits() override {
    // Note: Demo fan builds traits dynamically, so we store it as a member
    this->traits_ = fan::FanTraits{};

    // oscillation
    // speed
    // direction
    // speed_count
    switch (type_) {
      case DemoFanType::TYPE_1:
        break;
      case DemoFanType::TYPE_2:
        this->traits_.set_oscillation(true);
        break;
      case DemoFanType::TYPE_3:
        this->traits_.set_direction(true);
        this->traits_.set_speed(true);
        this->traits_.set_supported_speed_count(5);
        break;
      case DemoFanType::TYPE_4:
        this->traits_.set_direction(true);
        this->traits_.set_speed(true);
        this->traits_.set_supported_speed_count(100);
        this->traits_.set_oscillation(true);
        break;
    }

    return this->traits_;
  }

 protected:
  void control(const fan::FanCall &call) override {
    if (call.get_state().has_value())
      this->state = *call.get_state();
    if (call.get_oscillating().has_value())
      this->oscillating = *call.get_oscillating();
    if (call.get_speed().has_value())
      this->speed = *call.get_speed();
    if (call.get_direction().has_value())
      this->direction = *call.get_direction();

    this->publish_state();
  }

  DemoFanType type_;
  fan::FanTraits traits_;
};

}  // namespace demo
}  // namespace esphome

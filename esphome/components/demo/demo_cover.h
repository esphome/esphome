#pragma once

#include "esphome/core/component.h"
#include "esphome/components/cover/cover.h"

namespace esphome {
namespace demo {

enum class DemoCoverType {
  TYPE_1,
  TYPE_2,
  TYPE_3,
  TYPE_4,
};

class DemoCover : public cover::Cover, public Component {
 public:
  void set_type(DemoCoverType type) { type_ = type; }
  void setup() override {
    switch (type_) {
      case DemoCoverType::TYPE_1:
        this->position = cover::COVER_OPEN;
        break;
      case DemoCoverType::TYPE_2:
        this->position = 0.7;
        break;
      case DemoCoverType::TYPE_3:
        this->position = 0.1;
        this->tilt = 0.8;
        break;
      case DemoCoverType::TYPE_4:
        this->position = cover::COVER_CLOSED;
        this->tilt = 1.0;
        break;
    }
    this->publish_state();
  }

 protected:
  void control(const cover::CoverCall &call) override {
    if (call.get_position().has_value()) {
      float target = *call.get_position();
      this->current_operation =
          target > this->position ? cover::COVER_OPERATION_OPENING : cover::COVER_OPERATION_CLOSING;

      this->set_timeout("move", 2000, [this, target]() {
        this->current_operation = cover::COVER_OPERATION_IDLE;
        this->position = target;
        this->publish_state();
      });
    }
    if (call.get_tilt().has_value()) {
      this->tilt = *call.get_tilt();
    }
    if (call.get_stop()) {
      this->cancel_timeout("move");
    }

    this->publish_state();
  }
  cover::CoverTraits get_traits() override {
    cover::CoverTraits traits{};
    switch (type_) {
      case DemoCoverType::TYPE_1:
        traits.set_is_assumed_state(true);
        break;
      case DemoCoverType::TYPE_2:
        traits.set_supports_position(true);
        break;
      case DemoCoverType::TYPE_3:
        traits.set_supports_position(true);
        traits.set_supports_tilt(true);
        break;
      case DemoCoverType::TYPE_4:
        traits.set_supports_stop(true);
        traits.set_is_assumed_state(true);
        traits.set_supports_tilt(true);
        break;
    }
    return traits;
  }

  DemoCoverType type_;
};

}  // namespace demo
}  // namespace esphome

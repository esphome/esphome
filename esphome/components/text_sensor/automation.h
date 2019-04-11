#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace text_sensor {

class TextSensorStateTrigger : public Trigger<std::string> {
 public:
  explicit TextSensorStateTrigger(TextSensor *parent) {
    parent->add_on_state_callback([this](std::string value) { this->trigger(value); });
  }
};

template<typename... Ts> class TextSensorPublishAction : public Action<Ts...> {
 public:
  TextSensorPublishAction(TextSensor *sensor) : sensor_(sensor) {}
  TEMPLATABLE_VALUE(std::string, state)
  void play(Ts... x) override {
    this->sensor_->publish_state(this->state_.value(x...));
    this->play_next(x...);
  }

 protected:
  TextSensor *sensor_;
};

}  // namespace text_sensor
}  // namespace esphome

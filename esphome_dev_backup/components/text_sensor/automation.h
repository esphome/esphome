#pragma once

#include <utility>

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace text_sensor {

class TextSensorStateTrigger : public Trigger<std::string> {
 public:
  explicit TextSensorStateTrigger(TextSensor *parent) {
    parent->add_on_state_callback([this](const std::string &value) { this->trigger(value); });
  }
};

class TextSensorStateRawTrigger : public Trigger<std::string> {
 public:
  explicit TextSensorStateRawTrigger(TextSensor *parent) {
    parent->add_on_raw_state_callback([this](const std::string &value) { this->trigger(value); });
  }
};

template<typename... Ts> class TextSensorStateCondition : public Condition<Ts...> {
 public:
  explicit TextSensorStateCondition(TextSensor *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(std::string, state)

  bool check(Ts... x) override { return this->parent_->state == this->state_.value(x...); }

 protected:
  TextSensor *parent_;
};

template<typename... Ts> class TextSensorPublishAction : public Action<Ts...> {
 public:
  TextSensorPublishAction(TextSensor *sensor) : sensor_(sensor) {}
  TEMPLATABLE_VALUE(std::string, state)

  void play(Ts... x) override { this->sensor_->publish_state(this->state_.value(x...)); }

 protected:
  TextSensor *sensor_;
};

}  // namespace text_sensor
}  // namespace esphome

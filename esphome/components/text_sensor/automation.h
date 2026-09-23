#pragma once

#include <utility>

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::text_sensor {

class TextSensorStateTrigger final : public Trigger<std::string> {
 public:
  explicit TextSensorStateTrigger(TextSensor *parent) {
    parent->add_on_state_callback([this](const std::string &value) { this->trigger(value); });
  }
};

class TextSensorStateRawTrigger final : public Trigger<std::string> {
 public:
  explicit TextSensorStateRawTrigger(TextSensor *parent) {
    parent->add_on_raw_state_callback([this](const std::string &value) { this->trigger(value); });
  }
};

template<typename... Ts> class TextSensorStateCondition final : public Condition<Ts...> {
 public:
  explicit TextSensorStateCondition(TextSensor *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(std::string, state)

  bool check(const Ts &...x) override { return this->parent_->state == this->state_.value(x...); }

 protected:
  TextSensor *parent_;
};

}  // namespace esphome::text_sensor

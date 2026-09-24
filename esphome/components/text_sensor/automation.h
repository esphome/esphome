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

}  // namespace esphome::text_sensor

#pragma once

#include "esphome/core/component.h"

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/voice_assistant/voice_assistant.h"
namespace esphome {
namespace push_to_talk {

class PushToTalk : public Component, public voice_assistant::VoiceAssistant {
 public:
  PushToTalk();
  void setup() override;

  void set_binary_sensor(binary_sensor::BinarySensor *binary_sensor) { this->binary_sensor_ = binary_sensor; }

  float get_setup_priority() const override;

 protected:
  binary_sensor::BinarySensor *binary_sensor_{nullptr};
};

}  // namespace push_to_talk
}  // namespace esphome

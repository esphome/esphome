#pragma once

#include "esphome/core/component.h"
#include "esphome/components/cover/cover.h"
#include "esphome/components/motion_blinds/motion_blinds_communication.h"

#ifdef USE_ESP32

namespace esphome {
namespace motion_blinds {

class MotionBlindsComponent : public cover::Cover, public Component, public MotionBlindsCommunication {
 public:
  ~MotionBlindsComponent() override = default;
  void setup() override;
  void loop() override;
  void on_notify(const std::string &data) override;
  void on_disconnected() override;
  std::string get_logging_device_name() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  cover::CoverTraits get_traits() override;
  void set_invert_position(bool invert_position) { this->invert_position_ = invert_position; }

 protected:
  void control(const cover::CoverCall &call) override;
  float invert_position_value_(float value) { return this->invert_position_ ? 1.0f - value : value; }
  bool invert_position_;
};

}  // namespace motion_blinds
}  // namespace esphome

#endif

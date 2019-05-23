#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/cover/cover.h"

namespace esphome {
namespace tilt {

class TiltCover : public cover::Cover, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  Trigger<> *get_tilt_trigger() const { return this->tilt_trigger_; }
  void set_tilt_close_speed(float tilt_close_speed) { this->tilt_close_speed_ = tilt_close_speed; }
  void set_tilt_open_speed(float tilt_open_speed) { this->tilt_open_speed_ = tilt_open_speed; }
  cover::CoverTraits get_traits() override;

 protected:
  void control(const cover::CoverCall &call) override;
  void stop_prev_trigger_();
  bool is_at_target_() const;

  void start_direction_(cover::CoverOperation dir);

  void recompute_tilt_();

  float tilt_open_speed_;
  float tilt_close_speed_;
  Trigger<> *stop_trigger_{new Trigger<>()};

  Trigger<> *prev_command_trigger_{nullptr};
  uint32_t last_recompute_time_{0};
  uint32_t start_dir_time_{0};
  uint32_t last_publish_time_{0};
  float target_tilt_{0};
  Trigger<> *tilt_trigger_{new Trigger<>()};
};

}  // namespace tilt
}  // namespace esphome

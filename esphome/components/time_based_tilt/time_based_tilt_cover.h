#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/cover/cover.h"

namespace esphome {
namespace time_based_tilt {

class TimeBasedTiltCover : public cover::Cover, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  Trigger<> *get_open_trigger() const { return this->open_trigger_; }
  Trigger<> *get_close_trigger() const { return this->close_trigger_; }
  Trigger<> *get_stop_trigger() const { return this->stop_trigger_; }
  void set_open_duration(uint32_t open_duration) { this->open_duration_ = open_duration; }
  void set_close_duration(uint32_t close_duration) { this->close_duration_ = close_duration; }
  void set_tilt_open_duration(uint32_t tilt_open_duration) { this->tilt_open_duration_ = tilt_open_duration; }
  void set_tilt_close_duration(uint32_t tilt_close_duration) { this->tilt_close_duration_ = tilt_close_duration; }
  void set_interlock_wait_time(uint32_t interlock_wait_time) { this->interlock_wait_time_ = interlock_wait_time; }
  void set_recalibration_time(uint32_t recalibration_time) { this->recalibration_time_ = recalibration_time; }
  void set_inertia_open_time(uint32_t inertia_time) { this->inertia_open_time_ = inertia_time; }
  void set_inertia_close_time(uint32_t inertia_time) { this->inertia_close_time_ = inertia_time; }
  cover::CoverOperation compute_direction(float target, float current) { return target < current ? cover::COVER_OPERATION_CLOSING : cover::COVER_OPERATION_OPENING ; };
  float round_position(float pos) { return round(100 * pos)/100; };
  cover::CoverTraits get_traits() override;
  void set_assumed_state(bool value) { this->assumed_state_ = value; }

 protected:
  void control(const cover::CoverCall &call) override;
  bool is_at_target_position_() const;
  bool is_at_target_tilt_() const;

  Trigger<> *open_trigger_{new Trigger<>()};
  Trigger<> *close_trigger_{new Trigger<>()};
  Trigger<> *stop_trigger_{new Trigger<>()};

  uint32_t open_duration_;
  uint32_t close_duration_;

  uint32_t tilt_close_duration_;
  uint32_t tilt_open_duration_;

  uint32_t interlock_wait_time_;
  uint32_t recalibration_time_;
  uint32_t inertia_open_time_;
  uint32_t inertia_close_time_;

  const static float TARGET_NONE;
  enum State : uint8_t {
    STATE_IDLE,
    STATE_MOVING,
    STATE_STOPPING,
    STATE_CALIBRATING
  };

  uint32_t last_recompute_time_{0};
  uint32_t last_publish_time_{0};
  float target_position_{TARGET_NONE};
  float target_tilt_{TARGET_NONE};
  float inertia{0.0f};
  bool has_built_in_endstop_{false};
  bool assumed_state_{false};
  cover::CoverOperation last_operation_{cover::COVER_OPERATION_OPENING};
  State fsm_state_{STATE_IDLE};
  cover::CoverOperation interlocked_direction{cover::COVER_OPERATION_IDLE};
  uint32_t interlocked_time{0};
};

}  // namespace time_based_tilt
}  // namespace esphome

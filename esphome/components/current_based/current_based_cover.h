#pragma once

#include "esphome/components/cover/cover.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#ifdef USE_CURRENT_BASED_COVER_LEARN_DURATIONS
#include "esphome/core/preferences.h"
#endif
#include <cfloat>

namespace esphome::current_based {

#ifdef USE_CURRENT_BASED_COVER_LEARN_DURATIONS
struct LearnedDurations {
  uint32_t configured_open_duration;
  uint32_t configured_close_duration;
  uint32_t open_duration;
  uint32_t close_duration;
};
#endif

class CurrentBasedCover final : public cover::Cover, public Component {
  SUB_SENSOR(open)
  SUB_SENSOR(close)

 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  Trigger<> *get_stop_trigger() { return &this->stop_trigger_; }

  Trigger<> *get_open_trigger() { return &this->open_trigger_; }
  void set_open_moving_current_threshold(float open_moving_current_threshold) {
    this->open_moving_current_threshold_ = open_moving_current_threshold;
  }
  void set_open_obstacle_current_threshold(float open_obstacle_current_threshold) {
    this->open_obstacle_current_threshold_ = open_obstacle_current_threshold;
  }
  void set_open_duration(uint32_t open_duration) { this->open_duration_ = open_duration; }
  uint32_t get_open_duration() const { return this->open_duration_; }

  Trigger<> *get_close_trigger() { return &this->close_trigger_; }
  void set_close_moving_current_threshold(float close_moving_current_threshold) {
    this->close_moving_current_threshold_ = close_moving_current_threshold;
  }
  void set_close_obstacle_current_threshold(float close_obstacle_current_threshold) {
    this->close_obstacle_current_threshold_ = close_obstacle_current_threshold;
  }
  void set_close_duration(uint32_t close_duration) { this->close_duration_ = close_duration; }
  uint32_t get_close_duration() const { return this->close_duration_; }

  void set_max_duration(uint32_t max_duration) { this->max_duration_ = max_duration; }
  void set_obstacle_rollback(float obstacle_rollback) { this->obstacle_rollback_ = obstacle_rollback; }

  void set_malfunction_detection(bool malfunction_detection) { this->malfunction_detection_ = malfunction_detection; }
  void set_start_sensing_delay(uint32_t start_sensing_delay) { this->start_sensing_delay_ = start_sensing_delay; }
#ifdef USE_CURRENT_BASED_COVER_LEARN_DURATIONS
  void set_learn_durations(bool learn_durations) { this->learn_durations_ = learn_durations; }
#endif

  Trigger<> *get_malfunction_trigger() { return &this->malfunction_trigger_; }

  cover::CoverTraits get_traits() override;

 protected:
  void control(const cover::CoverCall &call) override;
  void stop_prev_trigger_();

  bool is_at_target_() const;
  bool is_opening_() const;
  bool is_opening_blocked_() const;
  bool is_closing_() const;
  bool is_closing_blocked_() const;
  bool is_initial_delay_finished_() const;

  void direction_idle_(float new_position = FLT_MAX);
  void start_direction_(cover::CoverOperation dir);

  void recompute_position_();
  void end_reached_(float position, uint32_t measured);
#ifdef USE_CURRENT_BASED_COVER_LEARN_DURATIONS
  void save_learned_durations_();
#endif

  Trigger<> stop_trigger_;

  Trigger<> open_trigger_;
  float open_moving_current_threshold_{0.0f};
  float open_obstacle_current_threshold_{FLT_MAX};
  uint32_t open_duration_{0};

  Trigger<> close_trigger_;
  float close_moving_current_threshold_{0.0f};
  float close_obstacle_current_threshold_{FLT_MAX};
  uint32_t close_duration_{0};

  uint32_t max_duration_{UINT32_MAX};
  bool malfunction_detection_{true};
#ifdef USE_CURRENT_BASED_COVER_LEARN_DURATIONS
  bool learn_durations_{false};
  bool stopped_at_end_{false};
  bool moving_from_end_{false};
  bool motor_current_seen_{false};
#endif
  Trigger<> malfunction_trigger_;
  uint32_t start_sensing_delay_{0};
  float obstacle_rollback_{0.0f};

#ifdef USE_CURRENT_BASED_COVER_LEARN_DURATIONS
  ESPPreferenceObject durations_pref_;
  uint32_t configured_open_duration_{0};
  uint32_t configured_close_duration_{0};
#endif

  Trigger<> *prev_command_trigger_{nullptr};
  uint32_t last_recompute_time_{0};
  uint32_t start_dir_time_{0};
  uint32_t last_publish_time_{0};
  float target_position_{0};

  cover::CoverOperation last_operation_{cover::COVER_OPERATION_OPENING};
};

}  // namespace esphome::current_based

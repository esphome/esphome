#pragma once

#include "esphome/components/cover/cover.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include <float.h>

namespace esphome {
namespace current_based {

class CurrentBasedCover : public cover::Cover, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  Trigger<> *get_stop_trigger() const { return this->stop_trigger_; }

  Trigger<> *get_open_trigger() const { return this->open_trigger_; }
  void set_open_sensor(sensor::Sensor *open_sensor) { this->open_sensor_ = open_sensor; }
  void set_open_moving_current_threshold(float open_moving_current_threshold) {
    this->open_moving_current_threshold_ = open_moving_current_threshold;
  }
  void set_open_obstacle_current_threshold(float open_obstacle_current_threshold) {
    this->open_obstacle_current_threshold_ = open_obstacle_current_threshold;
  }
  void set_open_duration(uint32_t open_duration) { this->open_duration_ = open_duration; }

  Trigger<> *get_close_trigger() const { return this->close_trigger_; }
  void set_close_sensor(sensor::Sensor *close_sensor) { this->close_sensor_ = close_sensor; }
  void set_close_moving_current_threshold(float close_moving_current_threshold) {
    this->close_moving_current_threshold_ = close_moving_current_threshold;
  }
  void set_close_obstacle_current_threshold(float close_obstacle_current_threshold) {
    this->close_obstacle_current_threshold_ = close_obstacle_current_threshold;
  }
  void set_close_duration(uint32_t close_duration) { this->close_duration_ = close_duration; }

  void set_max_duration(uint32_t max_duration) { this->max_duration_ = max_duration; }
  void set_obstacle_rollback(float obstacle_rollback) { this->obstacle_rollback_ = obstacle_rollback; }

  void set_interlock(bool interlock) { this->interlock_ = interlock; }
  void set_start_sensing_delay(uint32_t start_sensing_delay) { this->start_sensing_delay_ = start_sensing_delay; }

  cover::CoverTraits get_traits() override;

 protected:
  void control(const cover::CoverCall &call) override;
  void stop_prev_trigger_();
  bool is_at_target_() const;
  bool is_opening() const;
  bool is_opening_blocked() const;
  bool is_closing() const;
  bool is_closing_blocked() const;
  bool is_initial_delay_finished() const;
  /* bool is_moving() const;*/

  void start_direction_(cover::CoverOperation dir);

  void recompute_position_();

  sensor::Sensor *open_sensor_{nullptr};
  sensor::Sensor *close_sensor_{nullptr};

  Trigger<> *open_trigger_{new Trigger<>()};
  Trigger<> *close_trigger_{new Trigger<>()};
  Trigger<> *stop_trigger_{new Trigger<>()};

  float open_moving_current_threshold_;
  float close_moving_current_threshold_;

  float open_obstacle_current_threshold_{FLT_MAX};
  float close_obstacle_current_threshold_{FLT_MAX};
  
  uint32_t open_duration_;
  uint32_t close_duration_;

  uint32_t max_duration_{UINT32_MAX};
  bool interlock_{false};
  uint32_t start_sensing_delay_;
  float obstacle_rollback_;

  Trigger<> *prev_command_trigger_{nullptr};
  uint32_t last_recompute_time_{0};
  uint32_t start_dir_time_{0};
  uint32_t last_publish_time_{0};
  float target_position_{0};
};

}  // namespace current_based
}  // namespace esphome

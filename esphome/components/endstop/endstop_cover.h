#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/cover/cover.h"

namespace esphome::endstop {

class EndstopCover final : public cover::Cover, public Component {
  SUB_BINARY_SENSOR(open_endstop)
  SUB_BINARY_SENSOR(close_endstop)

 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  Trigger<> *get_open_trigger() { return &this->open_trigger_; }
  Trigger<> *get_close_trigger() { return &this->close_trigger_; }
  Trigger<> *get_stop_trigger() { return &this->stop_trigger_; }
  void set_open_duration(uint32_t open_duration) { this->open_duration_ = open_duration; }
  void set_close_duration(uint32_t close_duration) { this->close_duration_ = close_duration; }
  void set_max_duration(uint32_t max_duration) { this->max_duration_ = max_duration; }

  cover::CoverTraits get_traits() override;

 protected:
  void control(const cover::CoverCall &call) override;
  void stop_prev_trigger_();
  bool is_open_() const { return this->open_endstop_binary_sensor_->state; }
  bool is_closed_() const { return this->close_endstop_binary_sensor_->state; }
  bool is_at_target_() const;

  void start_direction_(cover::CoverOperation dir);

  void recompute_position_();

  Trigger<> open_trigger_;
  uint32_t open_duration_;
  Trigger<> close_trigger_;
  uint32_t close_duration_;
  Trigger<> stop_trigger_;
  uint32_t max_duration_{UINT32_MAX};

  Trigger<> *prev_command_trigger_{nullptr};
  uint32_t last_recompute_time_{0};
  uint32_t start_dir_time_{0};
  uint32_t last_publish_time_{0};
  float target_position_{0};
  cover::CoverOperation last_operation_{cover::COVER_OPERATION_OPENING};
};

}  // namespace esphome::endstop

#pragma once

#include "esphome/core/application.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/valve/valve.h"

namespace esphome::time_based {

enum TimeBasedValveRestoreMode {
  VALVE_NO_RESTORE,
  VALVE_RESTORE,
  VALVE_RESTORE_AND_CALL,
};

class TimeBasedValve final : public valve::Valve, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  Trigger<> *get_open_trigger();
  Trigger<> *get_close_trigger();
  Trigger<> *get_stop_trigger();
  void set_duration(uint32_t duration) { this->duration_ = duration; }
  void set_restore_mode(TimeBasedValveRestoreMode restore_mode) { restore_mode_ = restore_mode; }
  void reset_position();

 protected:
  void control(const valve::ValveCall &call) override;
  valve::ValveTraits get_traits() override;

  void stop_prev_trigger_();
  void start_direction_(valve::ValveOperation dir);
  bool is_at_target_() const;
  void recompute_position_();

  TimeBasedValveRestoreMode restore_mode_{VALVE_NO_RESTORE};
  uint32_t duration_;
  uint32_t last_publish_time_{0};
  uint32_t last_recompute_time_{0};
  valve::ValveOperation last_operation_{valve::VALVE_OPERATION_OPENING};
  float target_position_{0};
  float measured_position_{0};
  float measured_position_max_{0};
  float measured_position_min_{0};
  Trigger<> open_trigger_;
  Trigger<> close_trigger_;
  Trigger<> stop_trigger_;
  Trigger<> *prev_command_trigger_{nullptr};
};

}  // namespace esphome::time_based

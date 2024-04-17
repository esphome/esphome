#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/valve/valve.h"

namespace esphome {
namespace template_ {

enum TemplateValveRestoreMode {
  VALVE_NO_RESTORE,
  VALVE_RESTORE,
  VALVE_RESTORE_AND_CALL,
};

class TemplateValve : public valve::Valve, public Component {
 public:
  TemplateValve();

  void set_state_lambda(std::function<optional<float>()> &&f);
  Trigger<> *get_open_trigger() const;
  Trigger<> *get_close_trigger() const;
  Trigger<> *get_stop_trigger() const;
  Trigger<> *get_toggle_trigger() const;
  Trigger<float> *get_position_trigger() const;
  void set_optimistic(bool optimistic);
  void set_assumed_state(bool assumed_state);
  void set_has_stop(bool has_stop);
  void set_has_position(bool has_position);
  void set_has_toggle(bool has_toggle);
  void set_restore_mode(TemplateValveRestoreMode restore_mode) { restore_mode_ = restore_mode; }

  void setup() override;
  void loop() override;
  void dump_config() override;

  float get_setup_priority() const override;

 protected:
  void control(const valve::ValveCall &call) override;
  valve::ValveTraits get_traits() override;
  void stop_prev_trigger_();

  TemplateValveRestoreMode restore_mode_{VALVE_NO_RESTORE};
  optional<std::function<optional<float>()>> state_f_;
  bool assumed_state_{false};
  bool optimistic_{false};
  Trigger<> *open_trigger_;
  Trigger<> *close_trigger_;
  bool has_stop_{false};
  bool has_toggle_{false};
  Trigger<> *stop_trigger_;
  Trigger<> *toggle_trigger_;
  Trigger<> *prev_command_trigger_{nullptr};
  Trigger<float> *position_trigger_;
  bool has_position_{false};
};

}  // namespace template_
}  // namespace esphome

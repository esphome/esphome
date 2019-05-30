#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/cover/cover.h"

namespace esphome {
namespace template_ {

enum TemplateCoverRestoreMode {
  COVER_NO_RESTORE,
  COVER_RESTORE,
  COVER_RESTORE_AND_CALL,
};

class TemplateCover : public cover::Cover, public Component {
 public:
  TemplateCover();

  void set_state_lambda(std::function<optional<float>()> &&f);
  Trigger<> *get_open_trigger() const;
  Trigger<> *get_close_trigger() const;
  Trigger<> *get_stop_trigger() const;
  Trigger<float> *get_position_trigger() const;
  Trigger<float> *get_tilt_trigger() const;
  void set_optimistic(bool optimistic);
  void set_assumed_state(bool assumed_state);
  void set_tilt_lambda(std::function<optional<float>()> &&tilt_f);
  void set_has_position(bool has_position);
  void set_has_tilt(bool has_tilt);
  void set_restore_mode(TemplateCoverRestoreMode restore_mode) { restore_mode_ = restore_mode; }

  void setup() override;
  void loop() override;
  void dump_config() override;

  float get_setup_priority() const override;

 protected:
  void control(const cover::CoverCall &call) override;
  cover::CoverTraits get_traits() override;
  void stop_prev_trigger_();

  TemplateCoverRestoreMode restore_mode_{COVER_RESTORE};
  optional<std::function<optional<float>()>> state_f_;
  optional<std::function<optional<float>()>> tilt_f_;
  bool assumed_state_{false};
  bool optimistic_{false};
  Trigger<> *open_trigger_;
  Trigger<> *close_trigger_;
  Trigger<> *stop_trigger_;
  Trigger<> *prev_command_trigger_{nullptr};
  Trigger<float> *position_trigger_;
  bool has_position_{false};
  Trigger<float> *tilt_trigger_;
  bool has_tilt_{false};
};

}  // namespace template_
}  // namespace esphome

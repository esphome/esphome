#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/template_lambda.h"
#include "esphome/components/cover/cover.h"

namespace esphome::template_ {

enum TemplateCoverRestoreMode {
  COVER_NO_RESTORE,
  COVER_RESTORE,
  COVER_RESTORE_AND_CALL,
};

class TemplateCover final : public cover::Cover, public Component {
 public:
  TemplateCover();

  template<typename F> void set_state_lambda(F &&f) { this->state_f_.set(std::forward<F>(f)); }
  template<typename F> void set_tilt_lambda(F &&f) { this->tilt_f_.set(std::forward<F>(f)); }
  Trigger<> *get_open_trigger() const;
  Trigger<> *get_close_trigger() const;
  Trigger<> *get_stop_trigger() const;
  Trigger<> *get_toggle_trigger() const;
  Trigger<float> *get_position_trigger() const;
  Trigger<float> *get_tilt_trigger() const;
  void set_optimistic(bool optimistic);
  void set_assumed_state(bool assumed_state);
  void set_has_stop(bool has_stop);
  void set_has_position(bool has_position);
  void set_has_tilt(bool has_tilt);
  void set_has_toggle(bool has_toggle);
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
  TemplateLambda<float> state_f_;
  TemplateLambda<float> tilt_f_;
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
  Trigger<float> *tilt_trigger_;
  bool has_tilt_{false};
};

}  // namespace esphome::template_

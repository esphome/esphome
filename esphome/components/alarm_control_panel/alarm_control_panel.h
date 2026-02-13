#pragma once

#include "alarm_control_panel_call.h"
#include "alarm_control_panel_state.h"

#include "esphome/core/automation.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/log.h"

namespace esphome::alarm_control_panel {

enum AlarmControlPanelFeature : uint8_t {
  // Matches Home Assistant values
  ACP_FEAT_ARM_HOME = 1 << 0,
  ACP_FEAT_ARM_AWAY = 1 << 1,
  ACP_FEAT_ARM_NIGHT = 1 << 2,
  ACP_FEAT_TRIGGER = 1 << 3,
  ACP_FEAT_ARM_CUSTOM_BYPASS = 1 << 4,
  ACP_FEAT_ARM_VACATION = 1 << 5,
};

class AlarmControlPanel : public EntityBase {
 public:
  /** Make a AlarmControlPanelCall
   *
   */
  AlarmControlPanelCall make_call();

  /** Set the state of the alarm_control_panel.
   *
   * @param state The AlarmControlPanelState.
   */
  void publish_state(AlarmControlPanelState state);

  /** Add a callback for when the state of the alarm_control_panel changes.
   * Triggers can check get_state() to determine the new state.
   *
   * @param callback The callback function
   */
  void add_on_state_callback(std::function<void()> &&callback);

  /** Add a callback for when the state of the alarm_control_panel clears from triggered
   *
   * @param callback The callback function
   */
  void add_on_cleared_callback(std::function<void()> &&callback);

  /** Add a callback for when a chime zone goes from closed to open
   *
   * @param callback The callback function
   */
  void add_on_chime_callback(std::function<void()> &&callback);

  /** Add a callback for when a ready state changes
   *
   * @param callback The callback function
   */
  void add_on_ready_callback(std::function<void()> &&callback);

  /** A numeric representation of the supported features as per HomeAssistant
   *
   */
  virtual uint32_t get_supported_features() const = 0;

  /** Returns if the alarm_control_panel has a code
   *
   */
  virtual bool get_requires_code() const = 0;

  /** Returns if the alarm_control_panel requires a code to arm
   *
   */
  virtual bool get_requires_code_to_arm() const = 0;

  /** arm the alarm in away mode
   *
   * @param code The code
   */
  void arm_away(const char *code = nullptr);
  void arm_away(const optional<std::string> &code) {
    this->arm_away(code.has_value() ? code.value().c_str() : nullptr);
  }

  /** arm the alarm in home mode
   *
   * @param code The code
   */
  void arm_home(const char *code = nullptr);
  void arm_home(const optional<std::string> &code) {
    this->arm_home(code.has_value() ? code.value().c_str() : nullptr);
  }

  /** arm the alarm in night mode
   *
   * @param code The code
   */
  void arm_night(const char *code = nullptr);
  void arm_night(const optional<std::string> &code) {
    this->arm_night(code.has_value() ? code.value().c_str() : nullptr);
  }

  /** arm the alarm in vacation mode
   *
   * @param code The code
   */
  void arm_vacation(const char *code = nullptr);
  void arm_vacation(const optional<std::string> &code) {
    this->arm_vacation(code.has_value() ? code.value().c_str() : nullptr);
  }

  /** arm the alarm in custom bypass mode
   *
   * @param code The code
   */
  void arm_custom_bypass(const char *code = nullptr);
  void arm_custom_bypass(const optional<std::string> &code) {
    this->arm_custom_bypass(code.has_value() ? code.value().c_str() : nullptr);
  }

  /** disarm the alarm
   *
   * @param code The code
   */
  void disarm(const char *code = nullptr);
  void disarm(const optional<std::string> &code) { this->disarm(code.has_value() ? code.value().c_str() : nullptr); }

  /** Get the state
   *
   */
  AlarmControlPanelState get_state() const { return this->current_state_; }

  // is the state one of the armed states
  bool is_state_armed(AlarmControlPanelState state);

 protected:
  friend AlarmControlPanelCall;
  // Helper to reduce code duplication for arm/disarm methods
  void arm_with_code_(AlarmControlPanelCall &(AlarmControlPanelCall::*arm_method)(), const char *code);
  // in order to store last panel state in flash
  ESPPreferenceObject pref_;
  // current state
  AlarmControlPanelState current_state_;
  // the desired (or previous) state
  AlarmControlPanelState desired_state_;
  // last time the state was updated
  uint32_t last_update_;
  // the call control function
  virtual void control(const AlarmControlPanelCall &call) = 0;
  // state callback - triggers check get_state() for specific state
  LazyCallbackManager<void()> state_callback_{};
  // clear callback - fires when leaving TRIGGERED state
  LazyCallbackManager<void()> cleared_callback_{};
  // chime callback
  LazyCallbackManager<void()> chime_callback_{};
  // ready callback
  LazyCallbackManager<void()> ready_callback_{};
};

}  // namespace esphome::alarm_control_panel

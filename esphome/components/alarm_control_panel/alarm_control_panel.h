#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/entity_base.h"
#include <vector>
#include <map>

namespace esphome {
namespace alarm_control_panel {

enum AlarmControlPanelState : uint8_t {
  ACP_STATE_DISARMED = 0,
  ACP_STATE_ARMED_HOME = 1,
  ACP_STATE_ARMED_AWAY = 2,
  ACP_STATE_ARMED_NIGHT = 3,
  ACP_STATE_ARMED_VACATION = 4,
  ACP_STATE_ARMED_CUSTOM_BYPASS = 5,
  ACP_STATE_PENDING = 6,
  ACP_STATE_ARMING = 7,
  ACP_STATE_DISARMING = 8,
  ACP_STATE_TRIGGERED = 9
};

enum AlarmControlPanelFeature : uint8_t {
  // Matches Home Assistant values
  ACP_FEAT_ARM_HOME = 1,
  ACP_FEAT_ARM_AWAY = 2,
  ACP_FEAT_ARM_NIGHT = 4,
  ACP_FEAT_TRIGGER = 8,
  ACP_FEAT_ARM_CUSTOM_BYPASS = 16,
  ACP_FEAT_ARM_VACATION = 32
};

class AlarmControlPanel;

class AlarmControlPanelCall {
 public:
  AlarmControlPanelCall(AlarmControlPanel *parent);

  AlarmControlPanelCall &set_code(const std::string &code);
  AlarmControlPanelCall &arm_away();
  AlarmControlPanelCall &arm_home();
  AlarmControlPanelCall &arm_night();
  AlarmControlPanelCall &arm_vacation();
  AlarmControlPanelCall &arm_custom_bypass();
  AlarmControlPanelCall &disarm();
  AlarmControlPanelCall &pending();
  AlarmControlPanelCall &triggered();

  void perform();
  const optional<AlarmControlPanelState> &get_state() const;
  const optional<std::string> &get_code() const;

 protected:
  AlarmControlPanel *parent_;
  optional<std::string> code_{};
  optional<AlarmControlPanelState> state_{};
  void validate_();
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
  void set_panel_state(AlarmControlPanelState state);

  /** Returns a string representation of the state.
   *
   * @param state The AlarmControlPanelState.
   */
  std::string to_string(AlarmControlPanelState state);

  /** Add a callback for when the state of the alarm_control_panel changes
   *
   * @param callback The callback function
   */
  void add_on_state_callback(std::function<void()> &&callback);

  /** Add a callback for when the state of the alarm_control_panel chanes to triggered
   *
   * @param callback The callback function
   */
  void add_on_triggered_callback(std::function<void()> &&callback);

  /** Add a callback for when the state of the alarm_control_panel clears from triggered
   *
   * @param callback The callback function
   */
  void add_on_cleared_callback(std::function<void()> &&callback);

  /** A numeric representation of the supported features as per HomeAssistant
   *
   */
  virtual uint32_t get_supported_features();

  /** Returns if the alarm_control_panel has a code
   *
   */
  virtual bool get_requires_code();

  /** Returns if the alarm_control_panel requires a code to arm
   *
   */
  virtual bool get_requires_code_to_arm();

  /** arm the alarm in away mode
   *
   * @param code The code
   */
  void arm_away(optional<std::string> code = nullopt);

  /** arm the alarm in home mode
   *
   * @param code The code
   */
  void arm_home(optional<std::string> code = nullopt);

  /** arm the alarm in night mode
   *
   * @param code The code
   */
  void arm_night(optional<std::string> code = nullopt);

  /** arm the alarm in vacation mode
   *
   * @param code The code
   */
  void arm_vacation(optional<std::string> code = nullopt);

  /** arm the alarm in custom bypass mode
   *
   * @param code The code
   */
  void arm_custom_bypass(optional<std::string> code = nullopt);

  /** disarm the alarm
   *
   * @param code The code
   */
  void disarm(optional<std::string> code = nullopt);

  /** Get the state
   *
   */
  AlarmControlPanelState get_state();

  // is the state one of the armed states
  bool is_state_armed(AlarmControlPanelState state);

 protected:
  friend AlarmControlPanelCall;
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
  // state callback
  CallbackManager<void()> state_callback_{};
  // trigger callback
  CallbackManager<void()> triggered_callback_{};
  // clear callback
  CallbackManager<void()> cleared_callback_{};
};

}  // namespace alarm_control_panel
}  // namespace esphome

#pragma once

#include <map>

#include "alarm_control_panel_call.h"
#include "alarm_control_panel_state.h"

#include "esphome/core/automation.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/log.h"

namespace esphome {
namespace alarm_control_panel {

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

  /** Add a callback for when the state of the alarm_control_panel chanes to arming
   *
   * @param callback The callback function
   */
  void add_on_arming_callback(std::function<void()> &&callback);

  /** Add a callback for when the state of the alarm_control_panel changes to pending
   *
   * @param callback The callback function
   */
  void add_on_pending_callback(std::function<void()> &&callback);

  /** Add a callback for when the state of the alarm_control_panel changes to armed_home
   *
   * @param callback The callback function
   */
  void add_on_armed_home_callback(std::function<void()> &&callback);

  /** Add a callback for when the state of the alarm_control_panel changes to armed_night
   *
   * @param callback The callback function
   */
  void add_on_armed_night_callback(std::function<void()> &&callback);

  /** Add a callback for when the state of the alarm_control_panel changes to armed_away
   *
   * @param callback The callback function
   */
  void add_on_armed_away_callback(std::function<void()> &&callback);

  /** Add a callback for when the state of the alarm_control_panel changes to disarmed
   *
   * @param callback The callback function
   */
  void add_on_disarmed_callback(std::function<void()> &&callback);

  /** Add a callback for when the state of the alarm_control_panel clears from triggered
   *
   * @param callback The callback function
   */
  void add_on_cleared_callback(std::function<void()> &&callback);

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
  AlarmControlPanelState get_state() const { return this->current_state_; }

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
  // arming callback
  CallbackManager<void()> arming_callback_{};
  // pending callback
  CallbackManager<void()> pending_callback_{};
  // armed_home callback
  CallbackManager<void()> armed_home_callback_{};
  // armed_night callback
  CallbackManager<void()> armed_night_callback_{};
  // armed_away callback
  CallbackManager<void()> armed_away_callback_{};
  // disarmed callback
  CallbackManager<void()> disarmed_callback_{};
  // clear callback
  CallbackManager<void()> cleared_callback_{};
};

}  // namespace alarm_control_panel
}  // namespace esphome

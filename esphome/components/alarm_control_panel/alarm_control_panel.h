#pragma once

#include <vector>

#include "alarm_control_panel_call.h"
#include "alarm_control_panel_state.h"

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace alarm_control_panel {

/// Listener interface for alarm control panel state changes.
class AlarmControlPanelStateListener {
 public:
  virtual ~AlarmControlPanelStateListener() = default;
  /// Called when state changes. Check new_state/prev_state to filter specific states.
  virtual void on_state(AlarmControlPanelState new_state, AlarmControlPanelState prev_state) = 0;
};

/// Listener interface for alarm events (chime, ready, etc).
class AlarmControlPanelEventListener {
 public:
  virtual ~AlarmControlPanelEventListener() = default;
  /// Called when a chime zone opens while disarmed.
  virtual void on_chime() {}
  /// Called when zones ready state changes.
  virtual void on_ready() {}
};

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

  /** Register a listener for state changes.
   *
   * @param listener The listener to add (must remain valid for lifetime of panel)
   */
  void add_listener(AlarmControlPanelStateListener *listener) { this->state_listeners_.push_back(listener); }

  /** Register a listener for alarm events (chime/ready/etc).
   *
   * @param listener The listener to add (must remain valid for lifetime of panel)
   */
  void add_listener(AlarmControlPanelEventListener *listener) { this->event_listeners_.push_back(listener); }

  /** Notify listeners of a chime event (zone opened while disarmed).
   */
  void notify_chime();

  /** Notify listeners of a ready state change.
   */
  void notify_ready();

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
  // registered state listeners
  std::vector<AlarmControlPanelStateListener *> state_listeners_;
  // registered event listeners (chime/ready/etc)
  std::vector<AlarmControlPanelEventListener *> event_listeners_;
};

}  // namespace alarm_control_panel
}  // namespace esphome

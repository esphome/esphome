#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include <vector>
#include <map>

namespace esphome {
namespace alarm_control_panel {

enum AlarmControlPanelState : uint8_t {
  DISARMED = 0,
  ARMED_HOME = 1,
  ARMED_AWAY = 2,
  ARMED_NIGHT = 3,
  ARMED_VACATION = 4,
  ARMED_CUSTOM_BYPASS = 5,
  PENDING = 6,
  ARMING = 7,
  DISARMING = 8,
  TRIGGERED = 9
};

class AlarmControlPanel : public Component, public EntityBase {
 public:
  AlarmControlPanel();

  void setup() override;
  void loop() override;
  void dump_config() override;

  /** Add a binary_sensor to the alarm_panel.
   *
   * @param sensor The BinarySensor instance.
   * @param ignore_when_home if this should be ignored when armed_home mode
   */
  void add_sensor(binary_sensor::BinarySensor *sensor, bool bypass_when_home = false);

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

  /** A numeric representation of the supported features as per HomeAssistant
   *
   */
  uint32_t get_supported_features();

  /** set a code
   *
   * @param code The code
   */
  void set_code(std::string code);

  /** Returns if the alarm_control_panel has a code
   *
   */
  bool get_requires_code();

  /** set requires a code to arm
   *
   * @param code_to_arm The requires code to arm
   */
  void set_requires_code_to_arm(bool code_to_arm);

  /** Returns if the alarm_control_panel requires a code to arm
   *
   */
  bool get_requires_code_to_arm();

  /** set the delay (in seconds) before arming
   *
   * @param time The seconds
   */
  void set_arming_time(uint32_t time);

  /** set the delay (in seconds) before triggering
   *
   * @param time The seconds
   */
  void set_delay_time(uint32_t time);

  /** set the delay (in seconds) before resetting after triggered
   *
   * @param time The seconds
   */
  void set_trigger_time(uint32_t time);

  /** arm the alarm in away mode
   *
   * @param code The code
   */
  void arm_away(const std::string& code);

  /** arm the alarm in home mode
   *
   * @param code The code
   */
  void arm_home(const std::string& code);

  /** arm the alarm in night mode
   *
   * @param code The code
   */
  void arm_night(const std::string& code);

  /** arm the alarm in vacation mode
   *
   * @param code The code
   */
  void arm_vacation(const std::string& code);

  /** arm the alarm in custom bypass mode
   *
   * @param code The code
   */
  void arm_custom_bypass(const std::string& code);

  /** disarm the alarm
   *
   * @param code The code
   */
  void disarm(const std::string& code);

  /** Get the state
   *
   */
  AlarmControlPanelState get_state();

 protected:
  // in order to store last panel state in flash
  ESPPreferenceObject pref_;
  // the list of binary sensors that the alarm_panel monitors
  std::vector<binary_sensor::BinarySensor *> sensors_;
  // the list of sensor ids that the alarm_panel bypass when in armed_home
  std::vector<uint32_t> bypass_when_home_;
  // current state
  AlarmControlPanelState current_state_;
  // the desired (or previous) state
  AlarmControlPanelState desired_state_;
  // last time the state was updated
  uint32_t last_update_;
  // the arming delay
  uint32_t arming_time_;
  // the trigger delay
  uint32_t delay_time_;
  // the time in trigger
  uint32_t trigger_time_;
  // the code
  std::string code_ = "";
  // requires a code to arm
  bool requires_code_to_arm_ = false;
  // state callback
  CallbackManager<void()> state_callback_{};

  private:
    void arm_(const std::string& code, AlarmControlPanelState state);
};

}  // namespace alarm_control_panel
}  // namespace esphome

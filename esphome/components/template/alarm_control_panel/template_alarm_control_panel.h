#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/alarm_control_panel/alarm_control_panel.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace template_ {

class TemplateAlarmControlPanel : public alarm_control_panel::AlarmControlPanel, public Component {
 public:
  TemplateAlarmControlPanel();
  void dump_config() override;
  void setup() override;
  void loop() override;
  uint32_t get_supported_features() override;
  bool get_requires_code() override;
  bool get_requires_code_to_arm() override;

  /** Add a binary_sensor to the alarm_panel.
   *
   * @param sensor The BinarySensor instance.
   * @param ignore_when_home if this should be ignored when armed_home mode
   */
  void add_sensor(binary_sensor::BinarySensor *sensor, bool bypass_when_home = false);

  /** add a code
   *
   * @param code The code
   */
  void add_code(const std::string &code);

  /** set requires a code to arm
   *
   * @param code_to_arm The requires code to arm
   */
  void set_requires_code_to_arm(bool code_to_arm);

  /** set the delay before arming away
   *
   * @param time The milliseconds
   */
  void set_arming_away_time(uint32_t time);

  /** set the delay before arming home
   *
   * @param time The milliseconds
   */
  void set_arming_home_time(uint32_t time);

  /** set the delay before triggering
   *
   * @param time The milliseconds
   */
  void set_delay_time(uint32_t time);

  /** set the delay before resetting after triggered
   *
   * @param time The milliseconds
   */
  void set_trigger_time(uint32_t time);

 protected:
  void control(const alarm_control_panel::AlarmControlPanelCall &call) override;
  // the list of binary sensors that the alarm_panel monitors
  std::vector<binary_sensor::BinarySensor *> sensors_;
  // the list of sensor ids that the alarm_panel bypass when in armed_home
  std::vector<binary_sensor::BinarySensor *> bypass_when_home_;
  // the arming away delay
  uint32_t arming_away_time_;
  // the arming home delay
  uint32_t arming_home_time_;
  // the trigger delay
  uint32_t delay_time_;
  // the time in trigger
  uint32_t trigger_time_;
  // a list of codes
  std::vector<std::string> codes_;
  // requires a code to arm
  bool requires_code_to_arm_ = false;
  // check if the code is valid
  bool is_code_valid_(optional<std::string> code);

 private:
  void arm_(optional<std::string> code, alarm_control_panel::AlarmControlPanelState state, uint32_t delay);
};

}  // namespace template_
}  // namespace esphome

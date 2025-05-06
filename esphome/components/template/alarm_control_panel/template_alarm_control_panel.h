#pragma once

#include <cinttypes>
#include <map>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#include "esphome/components/alarm_control_panel/alarm_control_panel.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome {
namespace template_ {

#ifdef USE_BINARY_SENSOR
enum BinarySensorFlags : uint16_t {
  BINARY_SENSOR_MODE_NORMAL = 1 << 0,
  BINARY_SENSOR_MODE_BYPASS_ARMED_HOME = 1 << 1,
  BINARY_SENSOR_MODE_BYPASS_ARMED_NIGHT = 1 << 2,
  BINARY_SENSOR_MODE_CHIME = 1 << 3,
};

enum AlarmSensorType : uint16_t {
  ALARM_SENSOR_TYPE_DELAYED = 0,
  ALARM_SENSOR_TYPE_INSTANT,
  ALARM_SENSOR_TYPE_DELAYED_FOLLOWER,
  ALARM_SENSOR_TYPE_INSTANT_ALWAYS,
};

#endif

enum TemplateAlarmControlPanelRestoreMode {
  ALARM_CONTROL_PANEL_ALWAYS_DISARMED,
  ALARM_CONTROL_PANEL_RESTORE_DEFAULT_DISARMED,
};

struct SensorDataStore {
  bool last_chime_state;
};

struct SensorInfo {
  uint16_t flags;
  AlarmSensorType type;
  uint8_t store_index;
};

class TemplateAlarmControlPanel : public alarm_control_panel::AlarmControlPanel, public Component {
 public:
  TemplateAlarmControlPanel();
  void dump_config() override;
  void setup() override;
  void loop() override;
  uint32_t get_supported_features() const override;
  bool get_requires_code() const override;
  bool get_requires_code_to_arm() const override { return this->requires_code_to_arm_; }
  bool get_all_sensors_ready() { return this->sensors_ready_; };
  void set_restore_mode(TemplateAlarmControlPanelRestoreMode restore_mode) { this->restore_mode_ = restore_mode; }

#ifdef USE_BINARY_SENSOR
  /** Add a binary_sensor to the alarm_panel.
   *
   * @param sensor The BinarySensor instance.
   * @param ignore_when_home if this should be ignored when armed_home mode
   */
  void add_sensor(binary_sensor::BinarySensor *sensor, uint16_t flags = 0,
                  AlarmSensorType type = ALARM_SENSOR_TYPE_DELAYED);
#endif

  /** add a code
   *
   * @param code The code
   */
  void add_code(const std::string &code) { this->codes_.push_back(code); }

  /** set requires a code to arm
   *
   * @param code_to_arm The requires code to arm
   */
  void set_requires_code_to_arm(bool code_to_arm) { this->requires_code_to_arm_ = code_to_arm; }

  /** set the delay before arming away
   *
   * @param time The milliseconds
   */
  void set_arming_away_time(uint32_t time) { this->arming_away_time_ = time; }

  /** set the delay before arming home
   *
   * @param time The milliseconds
   */
  void set_arming_home_time(uint32_t time) { this->arming_home_time_ = time; }

  /** set the delay before arming night
   *
   * @param time The milliseconds
   */
  void set_arming_night_time(uint32_t time) { this->arming_night_time_ = time; }

  /** set the delay before triggering
   *
   * @param time The milliseconds
   */
  void set_pending_time(uint32_t time) { this->pending_time_ = time; }

  /** set the delay before resetting after triggered
   *
   * @param time The milliseconds
   */
  void set_trigger_time(uint32_t time) { this->trigger_time_ = time; }

  void set_supports_arm_home(bool supports_arm_home) { supports_arm_home_ = supports_arm_home; }

  void set_supports_arm_night(bool supports_arm_night) { supports_arm_night_ = supports_arm_night; }

 protected:
  void control(const alarm_control_panel::AlarmControlPanelCall &call) override;
#ifdef USE_BINARY_SENSOR
  // This maps a binary sensor to its type and attribute bits
  std::map<binary_sensor::BinarySensor *, SensorInfo> sensor_map_;

#endif
  TemplateAlarmControlPanelRestoreMode restore_mode_{};

  // the arming away delay
  uint32_t arming_away_time_;
  // the arming home delay
  uint32_t arming_home_time_{0};
  // the arming night delay
  uint32_t arming_night_time_{0};
  // the trigger delay
  uint32_t pending_time_;
  // the time in trigger
  uint32_t trigger_time_;
  // a list of codes
  std::vector<std::string> codes_;
  // Per sensor data store
  std::vector<SensorDataStore> sensor_data_;
  // requires a code to arm
  bool requires_code_to_arm_ = false;
  bool supports_arm_home_ = false;
  bool supports_arm_night_ = false;
  bool sensors_ready_ = false;
  bool sensors_ready_last_ = false;
  uint8_t next_store_index_ = 0;
  // check if the code is valid
  bool is_code_valid_(optional<std::string> code);

  void arm_(optional<std::string> code, alarm_control_panel::AlarmControlPanelState state, uint32_t delay);
};

}  // namespace template_
}  // namespace esphome

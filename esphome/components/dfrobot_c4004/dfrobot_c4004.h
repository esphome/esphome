#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include <cstdint>
#include <cstring>
#include <string>

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome {
namespace dfrobot_c4004 {

static const uint8_t MAX_TARGETS = 8;
static const uint8_t MAX_TAGS = 12;
static const uint8_t MAX_POINTS = 12;
static const uint8_t MAX_PAYLOAD = 180;
static const uint8_t QUERY_DATA = 0x0F;
static const uint8_t FRAME_HEAD1 = 0x53;
static const uint8_t FRAME_HEAD2 = 0x59;
static const uint8_t FRAME_TAIL1 = 0x54;
static const uint8_t FRAME_TAIL2 = 0x43;
static const uint16_t DEFAULT_TIMEOUT_MS = 200;
static const uint32_t HEARTBEAT_TIMEOUT_MS = 90000UL;

enum ReportedEvent : uint8_t {
  EVENT_NONE = 0x00,
  EVENT_TRAJECTORY = 0x01,
  EVENT_PRESENCE = 0x02,
  EVENT_MOTION = 0x03,
  EVENT_TAG = 0x04,
  EVENT_HEARTBEAT = 0x05,
  EVENT_INIT_FINISHED = 0x06,
  EVENT_PEOPLE_COUNT = 0x07,
  EVENT_UNKNOWN = 0xFE,
  EVENT_ERROR = 0xFF,
};

enum GetDataMode : uint8_t {
  GET_DATA_ACTIVE = 0x00,
  GET_DATA_REPORT = 0x01,
};

enum InstallMode : uint8_t {
  INSTALL_MODE_UNKNOWN = 0x00,
  INSTALL_MODE_SIDE = 0x01,
  INSTALL_MODE_TOP = 0x02,
};

enum PresenceState : uint8_t {
  NO_PRESENCE = 0x00,
  PRESENCE = 0x01,
  PRESENCE_UNKNOWN = 0xFF,
};

enum MotionState : uint8_t {
  MOTION_NONE = 0x00,
  MOTION_STATIC = 0x01,
  MOTION_ACTIVE = 0x02,
  MOTION_UNKNOWN = 0xFF,
};

enum DetectionRangeMode : uint8_t {
  RANGE_SIDE_DEFAULT = 0x00,
  RANGE_SIDE_LEFT_EDGE = 0x01,
  RANGE_SIDE_RIGHT_EDGE = 0x02,
  RANGE_HOTEL_CORRIDOR = 0x03,
  RANGE_FOUR_SIDE_BOUNDARY = 0x04,
  RANGE_TRAJECTORY = 0x05,
  RANGE_CONFIG_FILE = 0x06,
  RANGE_NO_BOUNDARY = 0x07,
  RANGE_TOP_DEFAULT = 0x08,
  RANGE_TOP_LEFT_EDGE = 0x09,
  RANGE_TOP_RIGHT_EDGE = 0x0A,
  RANGE_UNKNOWN = 0xFF,
};

enum ControlCode : uint8_t {
  CTRL_SYSTEM = 0x01,
  CTRL_WORK_STATUS = 0x05,
  CTRL_INSTALL_INFO = 0x06,
  CTRL_DETECTION_RANGE = 0x07,
  CTRL_PRESENCE = 0x80,
  CTRL_TRAJECTORY = 0x82,
  CTRL_PEOPLE_COUNT = 0x86,
};

enum CommandCode : uint8_t {
  CMD_SYSTEM_HEARTBEAT_REPORT = 0x01,
  CMD_SYSTEM_RESET = 0x02,
  CMD_SYSTEM_FACTORY_RESET = 0x03,
  CMD_SYSTEM_HEARTBEAT_QUERY = 0x80,
  CMD_WORK_STATUS_INIT_FINISHED_REPORT = 0x01,
  CMD_WORK_STATUS_INIT_FINISHED_QUERY = 0x81,
  CMD_INSTALL_SET_ANGLE = 0x01,
  CMD_INSTALL_SET_HEIGHT = 0x02,
  CMD_INSTALL_SET_MODE = 0x06,
  CMD_INSTALL_QUERY_ANGLE = 0x81,
  CMD_INSTALL_QUERY_HEIGHT = 0x82,
  CMD_INSTALL_QUERY_MODE = 0x86,
  CMD_PRESENCE_SET_ENABLE = 0x00,
  CMD_PRESENCE_REPORT = 0x01,
  CMD_PRESENCE_MOTION_REPORT = 0x02,
  CMD_PRESENCE_QUERY_ENABLE = 0x80,
  CMD_PRESENCE_QUERY_STATE = 0x81,
  CMD_PRESENCE_QUERY_MOTION = 0x82,
  CMD_TRAJECTORY_SET_ENABLE = 0x00,
  CMD_TRAJECTORY_TARGET_REPORT = 0x02,
  CMD_TRAJECTORY_QUERY_ENABLE = 0x80,
  CMD_TRAJECTORY_QUERY_TARGET = 0x82,
  CMD_TRAJECTORY_SET_TRAJECTORY_LED = 0x0B,
  CMD_TRAJECTORY_SET_MOTION_LED = 0x0C,
  CMD_TRAJECTORY_QUERY_TRAJECTORY_LED = 0x8B,
  CMD_TRAJECTORY_QUERY_MOTION_LED = 0x8C,
  CMD_DETECTION_RANGE_CLEAR_TAG = 0x13,
  CMD_DETECTION_RANGE_SET_RANGE = 0x1A,
  CMD_DETECTION_RANGE_QUERY_RANGE = 0x9A,
  CMD_DETECTION_RANGE_TAG_REPORT = 0x1B,
  CMD_PEOPLE_COUNT_REPORT = 0x0A,
  CMD_PEOPLE_COUNT_QUERY_COUNT = 0x8A,
  CMD_PEOPLE_COUNT_SET_REPORT_INTERVAL = 0x0B,
  CMD_PEOPLE_COUNT_QUERY_REPORT_INTERVAL = 0x8B,
  CMD_PEOPLE_COUNT_CLEAR_COUNT = 0x11,
  CMD_PEOPLE_COUNT_SET_TRAJECTORY_DISTANCE = 0x0E,
  CMD_PEOPLE_COUNT_QUERY_TRAJECTORY_DISTANCE = 0x8E,
  CMD_PEOPLE_COUNT_SET_TRAJECTORY_HOLD_TIME = 0x15,
  CMD_PEOPLE_COUNT_QUERY_TRAJECTORY_HOLD_TIME = 0x95,
  CMD_PEOPLE_COUNT_SET_NO_PERSON_DELAY = 0x17,
  CMD_PEOPLE_COUNT_QUERY_NO_PERSON_DELAY = 0x97,
};

struct InstallInfo {
  InstallMode mode{INSTALL_MODE_SIDE};
  uint16_t height_cm{180};
  int16_t x_angle{0};
  int16_t y_angle{0};
  int16_t z_angle{0};
};

struct BoundaryDetectionRange {
  DetectionRangeMode mode{RANGE_UNKNOWN};
  int16_t x_positive_cm{300};
  int16_t x_negative_cm{-300};
  int16_t y_positive_cm{500};
  int16_t y_negative_cm{0};
};

struct Packet {
  uint8_t control{0};
  uint8_t cmd{0};
  uint16_t len{0};
  uint8_t data[MAX_PAYLOAD]{};
};

class C4004Component : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

#ifdef USE_BINARY_SENSOR
  void set_online_binary_sensor(binary_sensor::BinarySensor *sensor) { this->online_binary_sensor_ = sensor; }
  void set_presence_binary_sensor(binary_sensor::BinarySensor *sensor) { this->presence_binary_sensor_ = sensor; }
#endif
#ifdef USE_SENSOR
  void set_people_count_sensor(sensor::Sensor *sensor) { this->people_count_sensor_ = sensor; }
  void set_motion_state_sensor(sensor::Sensor *sensor) { this->motion_state_sensor_ = sensor; }
#endif
#ifdef USE_TEXT_SENSOR
  void set_detection_range_mode_text_sensor(text_sensor::TextSensor *sensor) {
    this->detection_range_mode_text_sensor_ = sensor;
  }
  void set_status_text_sensor(text_sensor::TextSensor *sensor) { this->status_text_sensor_ = sensor; }
#endif
#ifdef USE_SWITCH
  void set_presence_enable_switch(switch_::Switch *sw) { this->presence_enable_switch_ = sw; }
  void set_trajectory_tracking_switch(switch_::Switch *sw) { this->trajectory_tracking_switch_ = sw; }
  void set_trajectory_led_switch(switch_::Switch *sw) { this->trajectory_led_switch_ = sw; }
  void set_motion_led_switch(switch_::Switch *sw) { this->motion_led_switch_ = sw; }
#endif
#ifdef USE_SELECT
  void set_install_mode_select(select::Select *select) { this->install_mode_select_ = select; }
#endif
#ifdef USE_NUMBER
  void set_install_height_number(number::Number *number) { this->install_height_number_ = number; }
  void set_install_z_angle_number(number::Number *number) { this->install_z_angle_number_ = number; }
  void set_range_x_max_number(number::Number *number) { this->range_x_max_number_ = number; }
  void set_range_x_min_number(number::Number *number) { this->range_x_min_number_ = number; }
  void set_range_y_max_number(number::Number *number) { this->range_y_max_number_ = number; }
  void set_range_y_min_number(number::Number *number) { this->range_y_min_number_ = number; }
  void set_target_count_number(number::Number *number) { this->target_count_number_ = number; }
  void set_people_report_interval_number(number::Number *number) { this->people_report_interval_number_ = number; }
  void set_trajectory_generate_distance_number(number::Number *number) {
    this->trajectory_generate_distance_number_ = number;
  }
  void set_trajectory_hold_time_number(number::Number *number) { this->trajectory_hold_time_number_ = number; }
  void set_no_person_delay_number(number::Number *number) { this->no_person_delay_number_ = number; }
#endif

  void set_pending_install_mode(const std::string &value);
  void set_pending_install_height(float value);
  void set_pending_install_z_angle(float value);
  void set_pending_range_x_max(float value);
  void set_pending_range_x_min(float value);
  void set_pending_range_y_max(float value);
  void set_pending_range_y_min(float value);

  bool reset_device();
  bool factory_reset();
  bool save_install_settings();
  bool apply_boundary_range();
  bool set_trajectory_range_mode();
  bool clear_all_tags();
  bool clear_people_count_command();
  bool write_presence_enable(bool enable);
  bool write_trajectory_tracking(bool enable);
  bool write_trajectory_led(bool enable);
  bool write_motion_led(bool enable);
  bool write_people_report_interval(float value);
  bool write_trajectory_generate_distance(float value);
  bool write_trajectory_hold_time(float value);
  bool write_no_person_delay(float value);

  float get_install_height() const { return this->install_info_.height_cm; }
  float get_install_z_angle() const { return this->install_info_.z_angle; }
  float get_range_x_max() const { return this->range_info_.x_positive_cm; }
  float get_range_x_min() const { return this->range_info_.x_negative_cm; }
  float get_range_y_max() const { return this->range_info_.y_positive_cm; }
  float get_range_y_min() const { return this->range_info_.y_negative_cm; }
  float get_target_count() const { return this->target_count_; }
  float get_people_report_interval() const { return this->people_report_interval_; }
  float get_trajectory_generate_distance() const { return this->trajectory_generate_distance_; }
  float get_trajectory_hold_time() const { return this->trajectory_hold_time_; }
  float get_no_person_delay() const { return this->no_person_delay_; }

  void publish_target_count_number();

 protected:
  bool begin();
  bool is_init_finished();
  bool is_connected();
  bool get_heartbeat(GetDataMode mode = GET_DATA_ACTIVE);
  ReportedEvent get_reported_info(uint16_t timeout_ms = 5);
  bool get_install_info(InstallInfo *info);
  bool set_install_info(const InstallInfo &info);
  bool get_presence_enable(bool *enable);
  PresenceState get_presence_state();
  MotionState get_motion_state();
  bool get_trajectory_tracking(bool *enable);
  bool get_trajectory_led(bool *enable);
  bool get_motion_led(bool *enable);
  uint8_t get_target_count_active();
  DetectionRangeMode get_detection_range_mode();
  bool get_boundary_detection_range(BoundaryDetectionRange *range);
  uint8_t get_people_count_info(GetDataMode mode = GET_DATA_ACTIVE);
  bool get_people_report_interval(uint32_t *value);
  bool get_trajectory_generate_distance(uint32_t *value);
  bool get_trajectory_hold_time(uint32_t *value);
  bool get_no_person_delay(uint32_t *value);

  bool set_byte(uint8_t control, uint8_t cmd, uint8_t value);
  bool query_byte(uint8_t control, uint8_t cmd, uint8_t *value);
  bool set_uint32(uint8_t control, uint8_t cmd, uint32_t value);
  bool query_uint32(uint8_t control, uint8_t cmd, uint32_t *value);
  bool send_command(uint8_t control, uint8_t cmd, const uint8_t *data, uint16_t len);
  bool request_frame(uint8_t control, uint8_t cmd, const uint8_t *data, uint16_t len, Packet *response,
                     uint16_t timeout_ms = DEFAULT_TIMEOUT_MS);
  bool read_frame(Packet *packet, uint16_t timeout_ms);
  bool read_byte(uint8_t *value, uint16_t timeout_ms);
  void flush_input();
  ReportedEvent handle_packet(const Packet *packet);
  ReportedEvent classify_packet(const Packet *packet) const;

  void parse_targets(const uint8_t *data, uint16_t len);
  void parse_boundary_range(const uint8_t *data, uint16_t len);
  void parse_people_count(const uint8_t *data, uint16_t len);

  uint16_t read_uint16(const uint8_t *data) const;
  int16_t read_int16(const uint8_t *data) const;
  int16_t read_sign_bit_int16(const uint8_t *data) const;
  uint32_t read_uint32(const uint8_t *data) const;
  void write_uint16(uint8_t *data, uint16_t value) const;
  void write_int16(uint8_t *data, int16_t value) const;
  void write_sign_bit_int16(uint8_t *data, int16_t value) const;
  void write_uint32(uint8_t *data, uint32_t value) const;

  void sync_device_state();
  void publish_all_states();
  void publish_online(bool online);
  void publish_presence_state();
  void publish_motion_state();
  void publish_people_count();
  void publish_install_info();
  void publish_boundary_range();
  void publish_switch_states();
  void publish_people_setting_numbers();
  void publish_detection_range_mode();
  void publish_status(const char *message);
  const char *install_mode_to_string(InstallMode mode) const;
  const char *range_mode_to_string(DetectionRangeMode mode) const;

  bool heartbeat_{false};
  bool init_finished_{false};
  bool connected_{false};
  bool presence_enable_{true};
  bool trajectory_tracking_{true};
  bool trajectory_led_{true};
  bool motion_led_{true};
  uint32_t last_heartbeat_ms_{0};
  uint32_t last_active_query_ms_{0};
  uint32_t last_heartbeat_query_ms_{0};
  InstallInfo install_info_{};
  BoundaryDetectionRange range_info_{};
  PresenceState presence_state_{PRESENCE_UNKNOWN};
  MotionState motion_state_{MOTION_UNKNOWN};
  uint8_t target_count_{0};
  uint8_t people_count_{0};
  uint32_t people_report_interval_{0};
  uint32_t trajectory_generate_distance_{0};
  uint32_t trajectory_hold_time_{0};
  uint32_t no_person_delay_{0};

#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *online_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *presence_binary_sensor_{nullptr};
#endif
#ifdef USE_SENSOR
  sensor::Sensor *people_count_sensor_{nullptr};
  sensor::Sensor *motion_state_sensor_{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *detection_range_mode_text_sensor_{nullptr};
  text_sensor::TextSensor *status_text_sensor_{nullptr};
#endif
#ifdef USE_SWITCH
  switch_::Switch *presence_enable_switch_{nullptr};
  switch_::Switch *trajectory_tracking_switch_{nullptr};
  switch_::Switch *trajectory_led_switch_{nullptr};
  switch_::Switch *motion_led_switch_{nullptr};
#endif
#ifdef USE_SELECT
  select::Select *install_mode_select_{nullptr};
#endif
#ifdef USE_NUMBER
  number::Number *install_height_number_{nullptr};
  number::Number *install_z_angle_number_{nullptr};
  number::Number *range_x_max_number_{nullptr};
  number::Number *range_x_min_number_{nullptr};
  number::Number *range_y_max_number_{nullptr};
  number::Number *range_y_min_number_{nullptr};
  number::Number *target_count_number_{nullptr};
  number::Number *people_report_interval_number_{nullptr};
  number::Number *trajectory_generate_distance_number_{nullptr};
  number::Number *trajectory_hold_time_number_{nullptr};
  number::Number *no_person_delay_number_{nullptr};
#endif
};

}  // namespace dfrobot_c4004
}  // namespace esphome

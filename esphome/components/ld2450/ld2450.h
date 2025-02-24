#pragma once

#include <iomanip>
#include <map>
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

#ifndef M_PI
#define M_PI 3.14
#endif

namespace esphome {
namespace ld2450 {

// Constants
static const uint8_t DEFAULT_PRESENCE_TIMEOUT = 5;  // Timeout to reset presense status 5 sec.
static const uint8_t MAX_LINE_LENGTH = 60;          // Max characters for serial buffer
static const uint8_t MAX_TARGETS = 3;               // Max 3 Targets in LD2450
static const uint8_t MAX_ZONES = 3;                 // Max 3 Zones in LD2450

// Target coordinate struct
struct Target {
  int16_t x;
  int16_t y;
  bool is_moving;
};

// Zone coordinate struct
struct Zone {
  int16_t x1 = 0;
  int16_t y1 = 0;
  int16_t x2 = 0;
  int16_t y2 = 0;
};

enum BaudRateStructure : uint8_t {
  BAUD_RATE_9600 = 1,
  BAUD_RATE_19200 = 2,
  BAUD_RATE_38400 = 3,
  BAUD_RATE_57600 = 4,
  BAUD_RATE_115200 = 5,
  BAUD_RATE_230400 = 6,
  BAUD_RATE_256000 = 7,
  BAUD_RATE_460800 = 8
};

// Convert baud rate enum to int
static const std::map<std::string, uint8_t> BAUD_RATE_ENUM_TO_INT{
    {"9600", BAUD_RATE_9600},     {"19200", BAUD_RATE_19200},   {"38400", BAUD_RATE_38400},
    {"57600", BAUD_RATE_57600},   {"115200", BAUD_RATE_115200}, {"230400", BAUD_RATE_230400},
    {"256000", BAUD_RATE_256000}, {"460800", BAUD_RATE_460800}};

// Zone type struct
enum ZoneTypeStructure : uint8_t { ZONE_DISABLED = 0, ZONE_DETECTION = 1, ZONE_FILTER = 2 };

// Convert zone type int to enum
static const std::map<ZoneTypeStructure, std::string> ZONE_TYPE_INT_TO_ENUM{
    {ZONE_DISABLED, "Disabled"}, {ZONE_DETECTION, "Detection"}, {ZONE_FILTER, "Filter"}};

// Convert zone type enum to int
static const std::map<std::string, uint8_t> ZONE_TYPE_ENUM_TO_INT{
    {"Disabled", ZONE_DISABLED}, {"Detection", ZONE_DETECTION}, {"Filter", ZONE_FILTER}};

// LD2450 serial command header & footer
static const uint8_t CMD_FRAME_HEADER[4] = {0xFD, 0xFC, 0xFB, 0xFA};
static const uint8_t CMD_FRAME_END[4] = {0x04, 0x03, 0x02, 0x01};

enum PeriodicDataStructure : uint8_t {
  TARGET_X = 4,
  TARGET_Y = 6,
  TARGET_SPEED = 8,
  TARGET_RESOLUTION = 10,
};

enum PeriodicDataValue : uint8_t { HEAD = 0XAA, END = 0x55, CHECK = 0x00 };

enum AckDataStructure : uint8_t { COMMAND = 6, COMMAND_STATUS = 7 };

class LD2450Component : public Component, public uart::UARTDevice {
#ifdef USE_SENSOR
  SUB_SENSOR(target_count)
  SUB_SENSOR(still_target_count)
  SUB_SENSOR(moving_target_count)
#endif
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(target)
  SUB_BINARY_SENSOR(moving_target)
  SUB_BINARY_SENSOR(still_target)
#endif
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(version)
  SUB_TEXT_SENSOR(mac)
#endif
#ifdef USE_SELECT
  SUB_SELECT(baud_rate)
  SUB_SELECT(zone_type)
#endif
#ifdef USE_SWITCH
  SUB_SWITCH(bluetooth)
  SUB_SWITCH(multi_target)
#endif
#ifdef USE_BUTTON
  SUB_BUTTON(reset)
  SUB_BUTTON(restart)
#endif
#ifdef USE_NUMBER
  SUB_NUMBER(presence_timeout)
#endif

 public:
  LD2450Component();
  void setup() override;
  void dump_config() override;
  void loop() override;
  void set_presence_timeout();
  void set_throttle(uint16_t value) { this->throttle_ = value; };
  void read_all_info();
  void query_zone_info();
  void restart_and_read_all_info();
  void set_bluetooth(bool enable);
  void set_multi_target(bool enable);
  void set_baud_rate(const std::string &state);
  void set_zone_type(const std::string &state);
  void publish_zone_type();
  void factory_reset();
#ifdef USE_TEXT_SENSOR
  void set_direction_text_sensor(uint8_t target, text_sensor::TextSensor *s);
#endif
#ifdef USE_NUMBER
  void set_zone_coordinate(uint8_t zone);
  void set_zone_x1_number(uint8_t zone, number::Number *n);
  void set_zone_y1_number(uint8_t zone, number::Number *n);
  void set_zone_x2_number(uint8_t zone, number::Number *n);
  void set_zone_y2_number(uint8_t zone, number::Number *n);
#endif
#ifdef USE_SENSOR
  void set_move_x_sensor(uint8_t target, sensor::Sensor *s);
  void set_move_y_sensor(uint8_t target, sensor::Sensor *s);
  void set_move_speed_sensor(uint8_t target, sensor::Sensor *s);
  void set_move_angle_sensor(uint8_t target, sensor::Sensor *s);
  void set_move_distance_sensor(uint8_t target, sensor::Sensor *s);
  void set_move_resolution_sensor(uint8_t target, sensor::Sensor *s);
  void set_zone_target_count_sensor(uint8_t zone, sensor::Sensor *s);
  void set_zone_still_target_count_sensor(uint8_t zone, sensor::Sensor *s);
  void set_zone_moving_target_count_sensor(uint8_t zone, sensor::Sensor *s);
#endif
  void reset_radar_zone();
  void set_radar_zone(int32_t zone_type, int32_t zone1_x1, int32_t zone1_y1, int32_t zone1_x2, int32_t zone1_y2,
                      int32_t zone2_x1, int32_t zone2_y1, int32_t zone2_x2, int32_t zone2_y2, int32_t zone3_x1,
                      int32_t zone3_y1, int32_t zone3_x2, int32_t zone3_y2);

 protected:
  void send_command_(uint8_t command_str, const uint8_t *command_value, uint8_t command_value_len);
  void set_config_mode_(bool enable);
  void handle_periodic_data_(uint8_t *buffer, uint8_t len);
  bool handle_ack_data_(uint8_t *buffer, uint8_t len);
  void process_zone_(uint8_t *buffer);
  void readline_(int readch, uint8_t *buffer, uint8_t len);
  void get_version_();
  void get_mac_();
  void query_target_tracking_mode_();
  void query_zone_();
  void restart_();
  void send_set_zone_command_();
  void save_to_flash_(float value);
  float restore_from_flash_();
  bool get_timeout_status_(uint32_t check_millis);
  uint8_t count_targets_in_zone_(const Zone &zone, bool is_moving);

  Target target_info_[MAX_TARGETS];
  Zone zone_config_[MAX_ZONES];
  uint8_t buffer_pos_ = 0;  // where to resume processing/populating buffer
  uint8_t buffer_data_[MAX_LINE_LENGTH];
  uint32_t last_periodic_millis_ = 0;
  uint32_t presence_millis_ = 0;
  uint32_t still_presence_millis_ = 0;
  uint32_t moving_presence_millis_ = 0;
  uint16_t throttle_ = 0;
  uint16_t timeout_ = 5;
  uint8_t zone_type_ = 0;
  std::string version_{};
  std::string mac_{};
#ifdef USE_NUMBER
  ESPPreferenceObject pref_;  // only used when numbers are in use
  std::vector<number::Number *> zone_x1_numbers_ = std::vector<number::Number *>(MAX_ZONES);
  std::vector<number::Number *> zone_y1_numbers_ = std::vector<number::Number *>(MAX_ZONES);
  std::vector<number::Number *> zone_x2_numbers_ = std::vector<number::Number *>(MAX_ZONES);
  std::vector<number::Number *> zone_y2_numbers_ = std::vector<number::Number *>(MAX_ZONES);
#endif
#ifdef USE_SENSOR
  std::vector<sensor::Sensor *> move_x_sensors_ = std::vector<sensor::Sensor *>(MAX_TARGETS);
  std::vector<sensor::Sensor *> move_y_sensors_ = std::vector<sensor::Sensor *>(MAX_TARGETS);
  std::vector<sensor::Sensor *> move_speed_sensors_ = std::vector<sensor::Sensor *>(MAX_TARGETS);
  std::vector<sensor::Sensor *> move_angle_sensors_ = std::vector<sensor::Sensor *>(MAX_TARGETS);
  std::vector<sensor::Sensor *> move_distance_sensors_ = std::vector<sensor::Sensor *>(MAX_TARGETS);
  std::vector<sensor::Sensor *> move_resolution_sensors_ = std::vector<sensor::Sensor *>(MAX_TARGETS);
  std::vector<sensor::Sensor *> zone_target_count_sensors_ = std::vector<sensor::Sensor *>(MAX_ZONES);
  std::vector<sensor::Sensor *> zone_still_target_count_sensors_ = std::vector<sensor::Sensor *>(MAX_ZONES);
  std::vector<sensor::Sensor *> zone_moving_target_count_sensors_ = std::vector<sensor::Sensor *>(MAX_ZONES);
#endif
#ifdef USE_TEXT_SENSOR
  std::vector<text_sensor::TextSensor *> direction_text_sensors_ = std::vector<text_sensor::TextSensor *>(3);
#endif
};

}  // namespace ld2450
}  // namespace esphome

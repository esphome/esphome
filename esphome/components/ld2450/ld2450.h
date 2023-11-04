#pragma once
#include "esphome/components/api/custom_api_device.h"
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
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
#include "esphome/components/uart/uart.h"
#include "esphome/core/helpers.h"
#include <map>
#include <sstream>
#include <iomanip>

#ifndef M_PI
#define M_PI 3.14
#endif

namespace esphome {
namespace ld2450 {

#define CHECK_BIT(var, pos) (((var) >> (pos)) & 1)

// Constants
static const uint16_t START_DELAY = 5000;           // Sensor startup delay 5 sec.
static const uint8_t DEFAULT_PRESENCE_TIMEOUT = 5;  // Timeout to reset presense status 5 sec.
static const uint8_t MAX_TARGETS = 3;               // Max 3 Targets in LD2450
static const uint8_t MAX_ZONES = 3;                 // Max 3 Zones in LD2450

// Zone coordinate config
struct Zone {
  int16_t x1 = 0;
  int16_t y1 = 0;
  int16_t x2 = 0;
  int16_t y2 = 0;
};

// Commands
static const uint8_t CMD_ENABLE_CONF = 0x00FF;
static const uint8_t CMD_DISABLE_CONF = 0x00FE;
static const uint8_t CMD_VERSION = 0x00A0;
static const uint8_t CMD_MAC = 0x00A5;
static const uint8_t CMD_RESET = 0x00A2;
static const uint8_t CMD_RESTART = 0x00A3;
static const uint8_t CMD_BLUETOOTH = 0x00A4;
static const uint8_t CMD_SINGLE_TARGET = 0x0080;
static const uint8_t CMD_MULTI_TARGET = 0x0090;
static const uint8_t CMD_SET_BAUD_RATE = 0x00A1;
static const uint8_t CMD_QUERY_ZONE = 0x00C1;
static const uint8_t CMD_SET_ZONE = 0x00C2;

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

static const std::map<std::string, uint8_t> BAUD_RATE_ENUM_TO_INT{
    {"9600", BAUD_RATE_9600},     {"19200", BAUD_RATE_19200},   {"38400", BAUD_RATE_38400},
    {"57600", BAUD_RATE_57600},   {"115200", BAUD_RATE_115200}, {"230400", BAUD_RATE_230400},
    {"256000", BAUD_RATE_256000}, {"460800", BAUD_RATE_460800}};

enum ZoneTypeStructure : uint8_t { ZONE_DISABLED = 0, ZONE_DETECTION = 1, ZONE_FILTER = 2 };

static const std::map<ZoneTypeStructure, std::string> ZONE_TYPE_INT_TO_ENUM{
    {ZONE_DISABLED, "Disabled"}, {ZONE_DETECTION, "Detection"}, {ZONE_FILTER, "Filter"}};

static const std::map<std::string, uint8_t> ZONE_TYPE_ENUM_TO_INT{
    {"Disabled", ZONE_DISABLED}, {"Detection", ZONE_DETECTION}, {"Filter", ZONE_FILTER}};

// Command Header & Footer
static const uint8_t CMD_FRAME_HEADER[4] = {0xFD, 0xFC, 0xFB, 0xFA};
static const uint8_t CMD_FRAME_END[4] = {0x04, 0x03, 0x02, 0x01};

// Data Header & Footer
static const uint8_t DATA_FRAME_HEADER[4] = {0xAA, 0xFF, 0x03, 0x00};
static const uint8_t DATA_FRAME_END[2] = {0x55, 0xCC};

enum PeriodicDataStructure : uint8_t {
  TARGET_X = 4,
  TARGET_Y = 6,
  TARGET_SPEED = 8,
  TARGET_RESOLUTION = 10,
};

enum PeriodicDataValue : uint8_t { HEAD = 0XAA, END = 0x55, CHECK = 0x00 };

enum AckDataStructure : uint8_t { COMMAND = 6, COMMAND_STATUS = 7 };

class LD2450Component : public Component, public api::CustomAPIDevice, public uart::UARTDevice {
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
  uint16_t convert_seconds_to_ms(uint16_t value) { return value * 1000; };
#ifdef USE_TEXT_SENSOR
  void set_direction_text_sensor(int target, text_sensor::TextSensor *s);
#endif
#ifdef USE_NUMBER
  void set_zone_coordinate(uint8_t zone);
  void set_zone_x1_number(int zone, number::Number *n);
  void set_zone_y1_number(int zone, number::Number *n);
  void set_zone_x2_number(int zone, number::Number *n);
  void set_zone_y2_number(int zone, number::Number *n);
#endif
#ifdef USE_SENSOR
  void set_move_x_sensor(int target, sensor::Sensor *s);
  void set_move_y_sensor(int target, sensor::Sensor *s);
  void set_move_speed_sensor(int target, sensor::Sensor *s);
  void set_move_angle_sensor(int target, sensor::Sensor *s);
  void set_move_distance_sensor(int target, sensor::Sensor *s);
  void set_move_resolution_sensor(int target, sensor::Sensor *s);
#endif

 protected:
  ESPPreferenceObject pref_;
  int two_byte_to_int_(char firstbyte, char secondbyte) { return (int16_t) (secondbyte << 8) + firstbyte; }
  void send_command_(uint8_t command_str, const uint8_t *command_value, int command_value_len);
  void set_config_mode_(bool enable);
  void handle_periodic_data_(uint8_t *buffer, int len);
  bool handle_ack_data_(uint8_t *buffer, int len);
  void process_zone_(uint8_t *buffer);
  void readline_(int readch, uint8_t *buffer, int len);
  void get_version_();
  void get_mac_();
  void query_zone_();
  void restart_();
  void send_set_zone_command_();
  void convert_int_values_to_hex_(const int *values, uint8_t *bytes);
  void on_reset_radar_zone_();
  void save_to_flash_(float value);
  float restore_from_flash_();
  Zone zone_config_[MAX_ZONES];
  void on_set_radar_zone_(int zone_type, int zone1_x1, int zone1_y1, int zone1_x2, int zone1_y2, int zone2_x1,
                          int zone2_y1, int zone2_x2, int zone2_y2, int zone3_x1, int zone3_y1, int zone3_x2,
                          int zone3_y2);
  int16_t decode_coordinate_(uint8_t low_byte, uint8_t high_byte) {
    int16_t coordinate = (high_byte & 0x7F) << 8 | low_byte;
    if ((high_byte & 0x80) == 0)
      coordinate = -coordinate;
    return coordinate;  // mm
  }
  int16_t decode_speed_(uint8_t low_byte, uint8_t high_byte) {
    int16_t speed = (high_byte & 0x7F) << 8 | low_byte;
    return speed * 10;  // mm/s
  }
  std::string convert_signed_int_to_hex_(int value) {
    std::stringstream ss;
    ss << std::hex << std::setw(4) << std::setfill('0') << (value & 0xFFFF);
    return ss.str();
  }
  int16_t hex_to_signed_int_(const uint8_t *buffer, uint8_t offset) {
    uint16_t hex_val = (buffer[offset + 1] << 8) | buffer[offset];
    int16_t dec_val = static_cast<int16_t>(hex_val);
    if (dec_val & 0x8000)
      dec_val -= 65536;
    return dec_val;
  }
  float calculate_angle_(float base, float hypotenuse) {
    if (base < 0.0 || hypotenuse <= 0.0)
      return 0.0;
    float angle_radians = std::acos(base / hypotenuse);
    float angle_degrees = angle_radians * (180.0 / M_PI);
    return angle_degrees;
  }
  std::string get_direction_(int16_t speed) {
    if (speed > 0)
      return "Moving away";
    if (speed < 0)
      return "Coming closer";
    return "Stationary";
  }
  int32_t uptime_millis_ = millis();
  int32_t last_periodic_millis_ = millis();
  int32_t presence_millis_ = 0;
  int32_t still_presence_millis_ = 0;
  int32_t moving_presence_millis_ = 0;
  uint16_t throttle_;
  uint16_t timeout_;
  uint8_t zone_type_ = 0;
  std::string version_;
  std::string mac_;
  bool get_timeout_status_(int32_t check_millis);
#ifdef USE_TEXT_SENSOR
  std::vector<text_sensor::TextSensor *> direction_text_sensors_ = std::vector<text_sensor::TextSensor *>(3);
#endif
#ifdef USE_NUMBER
  std::vector<number::Number *> zone_x1_numbers_ = std::vector<number::Number *>(3);
  std::vector<number::Number *> zone_y1_numbers_ = std::vector<number::Number *>(3);
  std::vector<number::Number *> zone_x2_numbers_ = std::vector<number::Number *>(3);
  std::vector<number::Number *> zone_y2_numbers_ = std::vector<number::Number *>(3);
#endif
#ifdef USE_SENSOR
  std::vector<sensor::Sensor *> move_x_sensors_ = std::vector<sensor::Sensor *>(3);
  std::vector<sensor::Sensor *> move_y_sensors_ = std::vector<sensor::Sensor *>(3);
  std::vector<sensor::Sensor *> move_speed_sensors_ = std::vector<sensor::Sensor *>(3);
  std::vector<sensor::Sensor *> move_angle_sensors_ = std::vector<sensor::Sensor *>(3);
  std::vector<sensor::Sensor *> move_distance_sensors_ = std::vector<sensor::Sensor *>(3);
  std::vector<sensor::Sensor *> move_resolution_sensors_ = std::vector<sensor::Sensor *>(3);
#endif
};

}  // namespace ld2450
}  // namespace esphome

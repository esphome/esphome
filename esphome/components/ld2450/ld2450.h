#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include <limits>
#include <cmath>
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
static const uint8_t MAX_LINE_LENGTH = 41;          // Max characters for serial buffer
static const uint8_t MAX_TARGETS = 3;               // Max 3 Targets in LD2450
static const uint8_t MAX_ZONES = 3;                 // Max 3 Zones in LD2450

enum Direction : uint8_t {
  DIRECTION_APPROACHING = 0,
  DIRECTION_MOVING_AWAY = 1,
  DIRECTION_STATIONARY = 2,
  DIRECTION_NA = 3,
  DIRECTION_UNDEFINED = 4,
};

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

#ifdef USE_NUMBER
struct ZoneOfNumbers {
  number::Number *x1 = nullptr;
  number::Number *y1 = nullptr;
  number::Number *x2 = nullptr;
  number::Number *y2 = nullptr;
};
#endif

class LD2450Component : public Component, public uart::UARTDevice {
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(moving_target)
  SUB_BINARY_SENSOR(still_target)
  SUB_BINARY_SENSOR(target)
#endif
#ifdef USE_SENSOR
  SUB_SENSOR(moving_target_count)
  SUB_SENSOR(still_target_count)
  SUB_SENSOR(target_count)
#endif
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(mac)
  SUB_TEXT_SENSOR(version)
#endif
#ifdef USE_NUMBER
  SUB_NUMBER(presence_timeout)
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
  SUB_BUTTON(factory_reset)
  SUB_BUTTON(restart)
#endif

 public:
  void setup() override;
  void dump_config() override;
  void loop() override;
  void set_presence_timeout();
  void set_throttle(uint16_t value) { this->throttle_ = value; }
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
  void set_zone_numbers(uint8_t zone, number::Number *x1, number::Number *y1, number::Number *x2, number::Number *y2);
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
  void handle_periodic_data_();
  bool handle_ack_data_();
  void process_zone_();
  void readline_(int readch);
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

  uint32_t last_periodic_millis_ = 0;
  uint32_t presence_millis_ = 0;
  uint32_t still_presence_millis_ = 0;
  uint32_t moving_presence_millis_ = 0;
  uint16_t throttle_ = 0;
  uint16_t timeout_ = 5;
  uint8_t buffer_data_[MAX_LINE_LENGTH];
  uint8_t mac_address_[6] = {0, 0, 0, 0, 0, 0};
  uint8_t version_[6] = {0, 0, 0, 0, 0, 0};
  uint8_t buffer_pos_ = 0;  // where to resume processing/populating buffer
  uint8_t zone_type_ = 0;
  bool bluetooth_on_{false};
  Target target_info_[MAX_TARGETS];
  Zone zone_config_[MAX_ZONES];

  // Change detection - cache previous values to avoid redundant publishes
  // All values are initialized to sentinel values that are outside the valid sensor ranges
  // to ensure the first real measurement is always published
  struct CachedTargetData {
    int16_t x = std::numeric_limits<int16_t>::min();             // -32768, outside range of -4860 to 4860
    int16_t y = std::numeric_limits<int16_t>::min();             // -32768, outside range of 0 to 7560
    int16_t speed = std::numeric_limits<int16_t>::min();         // -32768, outside practical sensor range
    uint16_t resolution = std::numeric_limits<uint16_t>::max();  // 65535, unlikely resolution value
    uint16_t distance = std::numeric_limits<uint16_t>::max();    // 65535, outside range of 0 to ~8990
    Direction direction = DIRECTION_UNDEFINED;                   // Undefined, will differ from any real direction
    float angle = NAN;                                           // NAN, safe sentinel for floats
  } cached_target_data_[MAX_TARGETS];

  struct CachedZoneData {
    uint8_t still_count = std::numeric_limits<uint8_t>::max();   // 255, unlikely zone count
    uint8_t moving_count = std::numeric_limits<uint8_t>::max();  // 255, unlikely zone count
    uint8_t total_count = std::numeric_limits<uint8_t>::max();   // 255, unlikely zone count
  } cached_zone_data_[MAX_ZONES];

  struct CachedGlobalData {
    uint8_t target_count = std::numeric_limits<uint8_t>::max();  // 255, max 3 targets possible
    uint8_t still_count = std::numeric_limits<uint8_t>::max();   // 255, max 3 targets possible
    uint8_t moving_count = std::numeric_limits<uint8_t>::max();  // 255, max 3 targets possible
  } cached_global_data_;

#ifdef USE_NUMBER
  ESPPreferenceObject pref_;  // only used when numbers are in use
  ZoneOfNumbers zone_numbers_[MAX_ZONES];
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

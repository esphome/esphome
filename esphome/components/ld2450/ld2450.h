#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
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

#include "esphome/components/ld24xx/ld24xx.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"

#include <array>

namespace esphome::ld2450 {

using namespace ld24xx;

// Constants
static constexpr uint8_t DEFAULT_PRESENCE_TIMEOUT = 5;  // Timeout to reset presense status 5 sec.
static constexpr uint8_t MAX_LINE_LENGTH = 41;          // Max characters for serial buffer
static constexpr uint8_t MAX_TARGETS = 3;               // Max 3 Targets in LD2450
static constexpr uint8_t MAX_ZONES = 3;                 // Max 3 Zones in LD2450

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
  SUB_SENSOR_WITH_DEDUP(moving_target_count, uint8_t)
  SUB_SENSOR_WITH_DEDUP(still_target_count, uint8_t)
  SUB_SENSOR_WITH_DEDUP(target_count, uint8_t)
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
  void read_all_info();
  void query_zone_info();
  void restart_and_read_all_info();
  void set_bluetooth(bool enable);
  void set_multi_target(bool enable);
  void set_baud_rate(const char *state);
  void set_zone_type(const char *state);
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

  uint32_t presence_millis_ = 0;
  uint32_t still_presence_millis_ = 0;
  uint32_t moving_presence_millis_ = 0;
  uint16_t timeout_ = 5;
  uint8_t buffer_data_[MAX_LINE_LENGTH];
  uint8_t mac_address_[6] = {0, 0, 0, 0, 0, 0};
  uint8_t version_[6] = {0, 0, 0, 0, 0, 0};
  uint8_t buffer_pos_ = 0;  // where to resume processing/populating buffer
  uint8_t zone_type_ = 0;
  bool bluetooth_on_{false};
  Target target_info_[MAX_TARGETS];
  Zone zone_config_[MAX_ZONES];

#ifdef USE_NUMBER
  ESPPreferenceObject pref_;  // only used when numbers are in use
  ZoneOfNumbers zone_numbers_[MAX_ZONES];
#endif
#ifdef USE_SENSOR
  std::array<SensorWithDedup<int16_t> *, MAX_TARGETS> move_x_sensors_{};
  std::array<SensorWithDedup<int16_t> *, MAX_TARGETS> move_y_sensors_{};
  std::array<SensorWithDedup<int16_t> *, MAX_TARGETS> move_speed_sensors_{};
  std::array<SensorWithDedup<float> *, MAX_TARGETS> move_angle_sensors_{};
  std::array<SensorWithDedup<uint16_t> *, MAX_TARGETS> move_distance_sensors_{};
  std::array<SensorWithDedup<uint16_t> *, MAX_TARGETS> move_resolution_sensors_{};
  std::array<SensorWithDedup<uint8_t> *, MAX_ZONES> zone_target_count_sensors_{};
  std::array<SensorWithDedup<uint8_t> *, MAX_ZONES> zone_still_target_count_sensors_{};
  std::array<SensorWithDedup<uint8_t> *, MAX_ZONES> zone_moving_target_count_sensors_{};
#endif
#ifdef USE_TEXT_SENSOR
  std::array<text_sensor::TextSensor *, 3> direction_text_sensors_{};
#endif
};

}  // namespace esphome::ld2450

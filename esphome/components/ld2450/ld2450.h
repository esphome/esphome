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

#ifdef LD2450_POLYGON_ZONE_COUNT
#include "polygon_zone.h"
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
// Zone query response is 40 bytes; +1 for null terminator, +4 so that a frame footer always
// lands inside the buffer during footer-based resynchronization after losing sync.
static constexpr uint8_t MAX_LINE_LENGTH = 45;
static constexpr uint8_t MAX_TARGETS = 3;  // Max 3 Targets in LD2450
static constexpr uint8_t MAX_ZONES = 3;    // Max 3 Zones in LD2450

// Silence after our own commands (restart, Bluetooth toggle, ...) is not a link loss; a restart takes about 2.3 s
static constexpr uint32_t LINK_GRACE_MS = 5000;
// Caps the windows, so retried commands cannot hide a real loss
static constexpr uint32_t LINK_GRACE_CAP_MS = 15000;

enum class LinkState : uint8_t {
  LINK_STATE_WAITING,  // no valid frame since boot
  LINK_STATE_UP,       // valid frames arrive
  LINK_STATE_LOST,     // no valid frame for the link timeout
};

enum Direction : uint8_t {
  DIRECTION_APPROACHING = 0,
  DIRECTION_MOVING_AWAY = 1,
  DIRECTION_STATIONARY = 2,
  DIRECTION_NA = 3,
  DIRECTION_UNDEFINED = 4,
};

enum BaudRate : uint8_t {
  BAUD_RATE_9600 = 1,
  BAUD_RATE_19200 = 2,
  BAUD_RATE_38400 = 3,
  BAUD_RATE_57600 = 4,
  BAUD_RATE_115200 = 5,
  BAUD_RATE_230400 = 6,
  BAUD_RATE_256000 = 7,
  BAUD_RATE_460800 = 8
};

enum ZoneType : uint8_t {
  ZONE_DISABLED = 0,
  ZONE_DETECTION = 1,
  ZONE_FILTER = 2,
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
  SUB_BINARY_SENSOR(connected)
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
  void set_link_timeout(uint32_t timeout_ms) { this->link_timeout_ms_ = timeout_ms; }
  void read_all_info();
  void query_zone_info();
  void restart_and_read_all_info();
  void set_bluetooth(bool enable);
  void set_multi_target(bool enable);
  void set_baud_rate(const char *state);
  void set_baud_rate(BaudRate rate);
  void set_zone_type(const char *state);
  void set_zone_type(ZoneType type);
  void publish_zone_type();
  void factory_reset();
#ifdef USE_TEXT_SENSOR
  void set_direction_text_sensor(uint8_t target, text_sensor::TextSensor *s);
#endif
#ifdef USE_LD2450_TARGET_XY
  void set_target_xy_text_sensor(uint8_t target, text_sensor::TextSensor *s);
#endif
#ifdef USE_NUMBER
  void set_zone_coordinate(number::Number *n);
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
#ifdef LD2450_POLYGON_ZONE_COUNT
  SLOT_LIST(PolygonZone, polygon_zones_, register_polygon_zone, LD2450_POLYGON_ZONE_COUNT)
#endif
  void reset_radar_zone();
  void set_radar_zone(int32_t zone_type, int32_t zone1_x1, int32_t zone1_y1, int32_t zone1_x2, int32_t zone1_y2,
                      int32_t zone2_x1, int32_t zone2_y1, int32_t zone2_x2, int32_t zone2_y2, int32_t zone3_x1,
                      int32_t zone3_y1, int32_t zone3_x2, int32_t zone3_y2);

  /// Add a callback that will be called after each successfully processed periodic data frame.
  template<typename F> void add_on_data_callback(F &&callback) { this->data_callback_.add(std::forward<F>(callback)); }

 protected:
  void send_command_(uint8_t command_str, const uint8_t *command_value, uint8_t command_value_len);
  void set_config_mode_(bool enable);
  void handle_periodic_data_();
  void handle_ack_data_();
  bool handle_frame_(uint8_t frame_len);
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
  void open_link_grace_();
  void check_link_();
  void on_valid_frame_();
  void on_link_lost_(uint32_t silent_ms, bool was_up);
  void on_link_up_(bool restored);
#ifdef USE_SENSOR
  void publish_target_unknown_(uint8_t target);
#endif
#ifdef USE_LD2450_TARGET_XY
  void update_target_xy_(uint8_t target, bool changed);
#endif
  void count_targets_in_zone_(const Zone &zone, uint8_t &still, uint8_t &moving);

  uint32_t presence_millis_ = 0;
  uint32_t still_presence_millis_ = 0;
  uint32_t moving_presence_millis_ = 0;
  uint32_t timeout_ = DEFAULT_PRESENCE_TIMEOUT * 1000;  // presence_timeout, in ms
  uint32_t link_timeout_ms_{0};
  uint32_t last_frame_ms_{0};     // last valid periodic frame; until the first one, when setup() ran
  uint32_t link_deadline_ms_{0};  // the link is lost when the loop clock reaches this
  uint8_t buffer_data_[MAX_LINE_LENGTH];
  uint8_t mac_address_[MAC_ADDRESS_SIZE] = {0, 0, 0, 0, 0, 0};
  uint8_t version_[6] = {0, 0, 0, 0, 0, 0};
  uint8_t buffer_pos_ = 0;  // where to resume processing/populating buffer
  uint8_t zone_type_ = 0;
  bool bluetooth_on_{false};
  LinkState link_state_{LinkState::LINK_STATE_WAITING};
  bool bytes_since_frame_{false};  // any bytes since the last valid frame
  uint16_t invalid_frames_{0};     // since the last valid frame, saturating
  Target target_info_[MAX_TARGETS];
  Zone zone_config_[MAX_ZONES];
#ifdef USE_TEXT_SENSOR
  // Byte aligned, so it packs after zone_config_ instead of padding before data_callback_
  std::array<Deduplicator<uint8_t>, MAX_TARGETS> direction_dedup_{};
#endif

#ifdef USE_NUMBER
  ESPPreferenceObject pref_;  // only used when numbers are in use
  ZoneOfNumbers zone_numbers_[MAX_ZONES];
#endif
#ifdef USE_SENSOR
  std::array<sensor::SensorWithDedup<int16_t>, MAX_TARGETS> move_x_sensors_{};
  std::array<sensor::SensorWithDedup<int16_t>, MAX_TARGETS> move_y_sensors_{};
  std::array<sensor::SensorWithDedup<int16_t>, MAX_TARGETS> move_speed_sensors_{};
  std::array<sensor::SensorWithDedup<float>, MAX_TARGETS> move_angle_sensors_{};
  std::array<sensor::SensorWithDedup<uint16_t>, MAX_TARGETS> move_distance_sensors_{};
  std::array<sensor::SensorWithDedup<uint16_t>, MAX_TARGETS> move_resolution_sensors_{};
  std::array<sensor::SensorWithDedup<uint8_t>, MAX_ZONES> zone_target_count_sensors_{};
  std::array<sensor::SensorWithDedup<uint8_t>, MAX_ZONES> zone_still_target_count_sensors_{};
  std::array<sensor::SensorWithDedup<uint8_t>, MAX_ZONES> zone_moving_target_count_sensors_{};
#endif
#ifdef USE_TEXT_SENSOR
  std::array<text_sensor::TextSensor *, MAX_TARGETS> direction_text_sensors_{};
#endif
#ifdef USE_LD2450_TARGET_XY
  // Publishes a target's position as "x,y" text, so both values always come from the same radar frame
  std::array<text_sensor::TextSensor *, MAX_TARGETS> target_xy_text_sensors_{};
#endif

  LazyCallbackManager<void()> data_callback_;
};

}  // namespace esphome::ld2450

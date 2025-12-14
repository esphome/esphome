#include "ld2450.h"

#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <cmath>
#include <numbers>

namespace esphome::ld2450 {

static const char *const TAG = "ld2450";

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

enum PeriodicData : uint8_t {
  TARGET_X = 4,
  TARGET_Y = 6,
  TARGET_SPEED = 8,
  TARGET_RESOLUTION = 10,
};

enum PeriodicDataValue : uint8_t {
  HEADER = 0xAA,
  FOOTER = 0x55,
  CHECK = 0x00,
};

enum AckData : uint8_t {
  COMMAND = 6,
  COMMAND_STATUS = 7,
};

// Memory-efficient lookup tables
struct StringToUint8 {
  const char *str;
  const uint8_t value;
};

struct Uint8ToString {
  const uint8_t value;
  const char *str;
};

constexpr StringToUint8 BAUD_RATES_BY_STR[] = {
    {"9600", BAUD_RATE_9600},     {"19200", BAUD_RATE_19200},   {"38400", BAUD_RATE_38400},
    {"57600", BAUD_RATE_57600},   {"115200", BAUD_RATE_115200}, {"230400", BAUD_RATE_230400},
    {"256000", BAUD_RATE_256000}, {"460800", BAUD_RATE_460800},
};

constexpr Uint8ToString DIRECTION_BY_UINT[] = {
    {DIRECTION_APPROACHING, "Approaching"},
    {DIRECTION_MOVING_AWAY, "Moving away"},
    {DIRECTION_STATIONARY, "Stationary"},
    {DIRECTION_NA, "NA"},
};

constexpr Uint8ToString ZONE_TYPE_BY_UINT[] = {
    {ZONE_DISABLED, "Disabled"},
    {ZONE_DETECTION, "Detection"},
    {ZONE_FILTER, "Filter"},
};

constexpr StringToUint8 ZONE_TYPE_BY_STR[] = {
    {"Disabled", ZONE_DISABLED},
    {"Detection", ZONE_DETECTION},
    {"Filter", ZONE_FILTER},
};

// Helper functions for lookups
template<size_t N> uint8_t find_uint8(const StringToUint8 (&arr)[N], const std::string &str) {
  for (const auto &entry : arr) {
    if (str == entry.str)
      return entry.value;
  }
  return 0xFF;  // Not found
}

template<size_t N> const char *find_str(const Uint8ToString (&arr)[N], uint8_t value) {
  for (const auto &entry : arr) {
    if (value == entry.value)
      return entry.str;
  }
  return "";  // Not found
}

// LD2450 UART Serial Commands
static constexpr uint8_t CMD_ENABLE_CONF = 0xFF;
static constexpr uint8_t CMD_DISABLE_CONF = 0xFE;
static constexpr uint8_t CMD_QUERY_VERSION = 0xA0;
static constexpr uint8_t CMD_QUERY_MAC_ADDRESS = 0xA5;
static constexpr uint8_t CMD_RESET = 0xA2;
static constexpr uint8_t CMD_RESTART = 0xA3;
static constexpr uint8_t CMD_BLUETOOTH = 0xA4;
static constexpr uint8_t CMD_SINGLE_TARGET_MODE = 0x80;
static constexpr uint8_t CMD_MULTI_TARGET_MODE = 0x90;
static constexpr uint8_t CMD_QUERY_TARGET_MODE = 0x91;
static constexpr uint8_t CMD_SET_BAUD_RATE = 0xA1;
static constexpr uint8_t CMD_QUERY_ZONE = 0xC1;
static constexpr uint8_t CMD_SET_ZONE = 0xC2;
// Header & Footer size
static constexpr uint8_t HEADER_FOOTER_SIZE = 4;
// Command Header & Footer
static constexpr uint8_t CMD_FRAME_HEADER[HEADER_FOOTER_SIZE] = {0xFD, 0xFC, 0xFB, 0xFA};
static constexpr uint8_t CMD_FRAME_FOOTER[HEADER_FOOTER_SIZE] = {0x04, 0x03, 0x02, 0x01};
// Data Header & Footer
static constexpr uint8_t DATA_FRAME_HEADER[HEADER_FOOTER_SIZE] = {0xAA, 0xFF, 0x03, 0x00};
static constexpr uint8_t DATA_FRAME_FOOTER[2] = {0x55, 0xCC};
// MAC address the module uses when Bluetooth is disabled
static constexpr uint8_t NO_MAC[] = {0x08, 0x05, 0x04, 0x03, 0x02, 0x01};

static inline uint16_t convert_seconds_to_ms(uint16_t value) { return value * 1000; };

static inline void convert_int_values_to_hex(const int *values, uint8_t *bytes) {
  for (uint8_t i = 0; i < 4; i++) {
    uint16_t val = values[i] & 0xFFFF;
    bytes[i * 2] = val & 0xFF;             // Store low byte first (little-endian)
    bytes[i * 2 + 1] = (val >> 8) & 0xFF;  // Store high byte second
  }
}

static inline int16_t decode_coordinate(uint8_t low_byte, uint8_t high_byte) {
  int16_t coordinate = (high_byte & 0x7F) << 8 | low_byte;
  if ((high_byte & 0x80) == 0) {
    coordinate = -coordinate;
  }
  return coordinate;  // mm
}

static inline int16_t decode_speed(uint8_t low_byte, uint8_t high_byte) {
  int16_t speed = (high_byte & 0x7F) << 8 | low_byte;
  if ((high_byte & 0x80) == 0) {
    speed = -speed;
  }
  return speed * 10;  // mm/s
}

static inline int16_t hex_to_signed_int(const uint8_t *buffer, uint8_t offset) {
  uint16_t hex_val = (buffer[offset + 1] << 8) | buffer[offset];
  int16_t dec_val = static_cast<int16_t>(hex_val);
  if (dec_val & 0x8000) {
    dec_val -= 65536;
  }
  return dec_val;
}

static inline float calculate_angle(float base, float hypotenuse) {
  if (base < 0.0f || hypotenuse <= 0.0f) {
    return 0.0f;
  }
  float angle_radians = acosf(base / hypotenuse);
  float angle_degrees = angle_radians * (180.0f / std::numbers::pi_v<float>);
  return angle_degrees;
}

static inline bool validate_header_footer(const uint8_t *header_footer, const uint8_t *buffer) {
  return std::memcmp(header_footer, buffer, HEADER_FOOTER_SIZE) == 0;
}

void LD2450Component::setup() {
#ifdef USE_NUMBER
  if (this->presence_timeout_number_ != nullptr) {
    this->pref_ = global_preferences->make_preference<float>(this->presence_timeout_number_->get_preference_hash());
    this->set_presence_timeout();
  }
#endif
  this->restart_and_read_all_info();
}

void LD2450Component::dump_config() {
  char mac_s[18];
  char version_s[20];
  const char *mac_str = ld24xx::format_mac_str(this->mac_address_, mac_s);
  ld24xx::format_version_str(this->version_, version_s);
  ESP_LOGCONFIG(TAG,
                "LD2450:\n"
                "  Firmware version: %s\n"
                "  MAC address: %s",
                version_s, mac_str);
#ifdef USE_BINARY_SENSOR
  ESP_LOGCONFIG(TAG, "Binary Sensors:");
  LOG_BINARY_SENSOR("  ", "MovingTarget", this->moving_target_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "StillTarget", this->still_target_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Target", this->target_binary_sensor_);
#endif
#ifdef USE_SENSOR
  ESP_LOGCONFIG(TAG, "Sensors:");
  LOG_SENSOR_WITH_DEDUP_SAFE("  ", "MovingTargetCount", this->moving_target_count_sensor_);
  LOG_SENSOR_WITH_DEDUP_SAFE("  ", "StillTargetCount", this->still_target_count_sensor_);
  LOG_SENSOR_WITH_DEDUP_SAFE("  ", "TargetCount", this->target_count_sensor_);
  for (auto &s : this->move_x_sensors_) {
    LOG_SENSOR_WITH_DEDUP_SAFE("  ", "TargetX", s);
  }
  for (auto &s : this->move_y_sensors_) {
    LOG_SENSOR_WITH_DEDUP_SAFE("  ", "TargetY", s);
  }
  for (auto &s : this->move_angle_sensors_) {
    LOG_SENSOR_WITH_DEDUP_SAFE("  ", "TargetAngle", s);
  }
  for (auto &s : this->move_distance_sensors_) {
    LOG_SENSOR_WITH_DEDUP_SAFE("  ", "TargetDistance", s);
  }
  for (auto &s : this->move_resolution_sensors_) {
    LOG_SENSOR_WITH_DEDUP_SAFE("  ", "TargetResolution", s);
  }
  for (auto &s : this->move_speed_sensors_) {
    LOG_SENSOR_WITH_DEDUP_SAFE("  ", "TargetSpeed", s);
  }
  for (auto &s : this->zone_target_count_sensors_) {
    LOG_SENSOR_WITH_DEDUP_SAFE("  ", "ZoneTargetCount", s);
  }
  for (auto &s : this->zone_moving_target_count_sensors_) {
    LOG_SENSOR_WITH_DEDUP_SAFE("  ", "ZoneMovingTargetCount", s);
  }
  for (auto &s : this->zone_still_target_count_sensors_) {
    LOG_SENSOR_WITH_DEDUP_SAFE("  ", "ZoneStillTargetCount", s);
  }
#endif
#ifdef USE_TEXT_SENSOR
  ESP_LOGCONFIG(TAG, "Text Sensors:");
  LOG_TEXT_SENSOR("  ", "Version", this->version_text_sensor_);
  LOG_TEXT_SENSOR("  ", "MAC address", this->mac_text_sensor_);
  for (text_sensor::TextSensor *s : this->direction_text_sensors_) {
    LOG_TEXT_SENSOR("  ", "Direction", s);
  }
#endif
#ifdef USE_NUMBER
  ESP_LOGCONFIG(TAG, "Numbers:");
  LOG_NUMBER("  ", "PresenceTimeout", this->presence_timeout_number_);
  for (auto n : this->zone_numbers_) {
    LOG_NUMBER("  ", "ZoneX1", n.x1);
    LOG_NUMBER("  ", "ZoneY1", n.y1);
    LOG_NUMBER("  ", "ZoneX2", n.x2);
    LOG_NUMBER("  ", "ZoneY2", n.y2);
  }
#endif
#ifdef USE_SELECT
  ESP_LOGCONFIG(TAG, "Selects:");
  LOG_SELECT("  ", "BaudRate", this->baud_rate_select_);
  LOG_SELECT("  ", "ZoneType", this->zone_type_select_);
#endif
#ifdef USE_SWITCH
  ESP_LOGCONFIG(TAG, "Switches:");
  LOG_SWITCH("  ", "Bluetooth", this->bluetooth_switch_);
  LOG_SWITCH("  ", "MultiTarget", this->multi_target_switch_);
#endif
#ifdef USE_BUTTON
  ESP_LOGCONFIG(TAG, "Buttons:");
  LOG_BUTTON("  ", "FactoryReset", this->factory_reset_button_);
  LOG_BUTTON("  ", "Restart", this->restart_button_);
#endif
}

void LD2450Component::loop() {
  while (this->available()) {
    this->readline_(this->read());
  }
}

// Count targets in zone
uint8_t LD2450Component::count_targets_in_zone_(const Zone &zone, bool is_moving) {
  uint8_t count = 0;
  for (auto &index : this->target_info_) {
    if (index.x > zone.x1 && index.x < zone.x2 && index.y > zone.y1 && index.y < zone.y2 &&
        index.is_moving == is_moving) {
      count++;
    }
  }
  return count;
}

// Service reset_radar_zone
void LD2450Component::reset_radar_zone() {
  this->zone_type_ = 0;
  for (auto &i : this->zone_config_) {
    i.x1 = 0;
    i.y1 = 0;
    i.x2 = 0;
    i.y2 = 0;
  }
  this->send_set_zone_command_();
}

void LD2450Component::set_radar_zone(int32_t zone_type, int32_t zone1_x1, int32_t zone1_y1, int32_t zone1_x2,
                                     int32_t zone1_y2, int32_t zone2_x1, int32_t zone2_y1, int32_t zone2_x2,
                                     int32_t zone2_y2, int32_t zone3_x1, int32_t zone3_y1, int32_t zone3_x2,
                                     int32_t zone3_y2) {
  this->zone_type_ = zone_type;
  int zone_parameters[12] = {zone1_x1, zone1_y1, zone1_x2, zone1_y2, zone2_x1, zone2_y1,
                             zone2_x2, zone2_y2, zone3_x1, zone3_y1, zone3_x2, zone3_y2};
  for (uint8_t i = 0; i < MAX_ZONES; i++) {
    this->zone_config_[i].x1 = zone_parameters[i * 4];
    this->zone_config_[i].y1 = zone_parameters[i * 4 + 1];
    this->zone_config_[i].x2 = zone_parameters[i * 4 + 2];
    this->zone_config_[i].y2 = zone_parameters[i * 4 + 3];
  }
  this->send_set_zone_command_();
}

// Set Zone on LD2450 Sensor
void LD2450Component::send_set_zone_command_() {
  uint8_t cmd_value[26] = {};
  uint8_t zone_type_bytes[2] = {static_cast<uint8_t>(this->zone_type_), 0x00};
  uint8_t area_config[24] = {};
  for (uint8_t i = 0; i < MAX_ZONES; i++) {
    int values[4] = {this->zone_config_[i].x1, this->zone_config_[i].y1, this->zone_config_[i].x2,
                     this->zone_config_[i].y2};
    ld2450::convert_int_values_to_hex(values, area_config + (i * 8));
  }
  std::memcpy(cmd_value, zone_type_bytes, sizeof(zone_type_bytes));
  std::memcpy(cmd_value + 2, area_config, sizeof(area_config));
  this->set_config_mode_(true);
  this->send_command_(CMD_SET_ZONE, cmd_value, sizeof(cmd_value));
  this->set_config_mode_(false);
}

// Check presense timeout to reset presence status
bool LD2450Component::get_timeout_status_(uint32_t check_millis) {
  if (check_millis == 0) {
    return true;
  }
  if (this->timeout_ == 0) {
    this->timeout_ = ld2450::convert_seconds_to_ms(DEFAULT_PRESENCE_TIMEOUT);
  }
  return App.get_loop_component_start_time() - check_millis >= this->timeout_;
}

// Extract, store and publish zone details LD2450 buffer
void LD2450Component::process_zone_() {
  uint8_t index, start;
  for (index = 0; index < MAX_ZONES; index++) {
    start = 12 + index * 8;
    this->zone_config_[index].x1 = ld2450::hex_to_signed_int(this->buffer_data_, start);
    this->zone_config_[index].y1 = ld2450::hex_to_signed_int(this->buffer_data_, start + 2);
    this->zone_config_[index].x2 = ld2450::hex_to_signed_int(this->buffer_data_, start + 4);
    this->zone_config_[index].y2 = ld2450::hex_to_signed_int(this->buffer_data_, start + 6);
#ifdef USE_NUMBER
    // only one null check as all coordinates are required for a single zone
    if (this->zone_numbers_[index].x1 != nullptr) {
      this->zone_numbers_[index].x1->publish_state(this->zone_config_[index].x1);
      this->zone_numbers_[index].y1->publish_state(this->zone_config_[index].y1);
      this->zone_numbers_[index].x2->publish_state(this->zone_config_[index].x2);
      this->zone_numbers_[index].y2->publish_state(this->zone_config_[index].y2);
    }
#endif
  }
}

// Read all info from LD2450 buffer
void LD2450Component::read_all_info() {
  this->set_config_mode_(true);
  this->get_version_();
  this->get_mac_();
  this->query_target_tracking_mode_();
  this->query_zone_();
  this->set_config_mode_(false);
#ifdef USE_SELECT
  const auto baud_rate = std::to_string(this->parent_->get_baud_rate());
  if (this->baud_rate_select_ != nullptr && strcmp(this->baud_rate_select_->current_option(), baud_rate.c_str()) != 0) {
    this->baud_rate_select_->publish_state(baud_rate);
  }
  this->publish_zone_type();
#endif
}

// Read zone info from LD2450 buffer
void LD2450Component::query_zone_info() {
  this->set_config_mode_(true);
  this->query_zone_();
  this->set_config_mode_(false);
}

// Restart LD2450 and read all info from buffer
void LD2450Component::restart_and_read_all_info() {
  this->set_config_mode_(true);
  this->restart_();
  this->set_timeout(1500, [this]() { this->read_all_info(); });
}

// Send command with values to LD2450
void LD2450Component::send_command_(uint8_t command, const uint8_t *command_value, uint8_t command_value_len) {
  ESP_LOGV(TAG, "Sending COMMAND %02X", command);
  // frame header bytes
  this->write_array(CMD_FRAME_HEADER, sizeof(CMD_FRAME_HEADER));
  // length bytes
  uint8_t len = 2;
  if (command_value != nullptr) {
    len += command_value_len;
  }
  // 2 length bytes (low, high) + 2 command bytes (low, high)
  uint8_t len_cmd[] = {len, 0x00, command, 0x00};
  this->write_array(len_cmd, sizeof(len_cmd));
  // command value bytes
  if (command_value != nullptr) {
    this->write_array(command_value, command_value_len);
  }
  // frame footer bytes
  this->write_array(CMD_FRAME_FOOTER, sizeof(CMD_FRAME_FOOTER));

  if (command != CMD_ENABLE_CONF && command != CMD_DISABLE_CONF) {
    delay(50);  // NOLINT
  }
}

// LD2450 Radar data message:
//  [AA FF 03 00] [0E 03 B1 86 10 00 40 01] [00 00 00 00 00 00 00 00] [00 00 00 00 00 00 00 00] [55 CC]
//   Header       Target 1                  Target 2                  Target 3                  End
void LD2450Component::handle_periodic_data_() {
  if (this->buffer_pos_ < 29) {  // header (4 bytes) + 8 x 3 target data + footer (2 bytes)
    ESP_LOGE(TAG, "Invalid length");
    return;
  }
  if (!ld2450::validate_header_footer(DATA_FRAME_HEADER, this->buffer_data_) ||
      this->buffer_data_[this->buffer_pos_ - 2] != DATA_FRAME_FOOTER[0] ||
      this->buffer_data_[this->buffer_pos_ - 1] != DATA_FRAME_FOOTER[1]) {
    ESP_LOGE(TAG, "Invalid header/footer");
    return;
  }

  int16_t target_count = 0;
  int16_t still_target_count = 0;
  int16_t moving_target_count = 0;
  int16_t res = 0;
  int16_t start = 0;
  int16_t tx = 0;
  int16_t ty = 0;
  int16_t td = 0;
  int16_t ts = 0;
  int16_t angle = 0;
  uint8_t index = 0;
  Direction direction{DIRECTION_UNDEFINED};
  bool is_moving = false;

#if defined(USE_BINARY_SENSOR) || defined(USE_SENSOR) || defined(USE_TEXT_SENSOR)
  // Loop thru targets
  for (index = 0; index < MAX_TARGETS; index++) {
#ifdef USE_SENSOR
    // X
    start = TARGET_X + index * 8;
    is_moving = false;
    // tx is used for further calculations, so always needs to be populated
    tx = ld2450::decode_coordinate(this->buffer_data_[start], this->buffer_data_[start + 1]);
    SAFE_PUBLISH_SENSOR(this->move_x_sensors_[index], tx);
    // Y
    start = TARGET_Y + index * 8;
    ty = ld2450::decode_coordinate(this->buffer_data_[start], this->buffer_data_[start + 1]);
    SAFE_PUBLISH_SENSOR(this->move_y_sensors_[index], ty);
    // RESOLUTION
    start = TARGET_RESOLUTION + index * 8;
    res = (this->buffer_data_[start + 1] << 8) | this->buffer_data_[start];
    SAFE_PUBLISH_SENSOR(this->move_resolution_sensors_[index], res);
#endif
    // SPEED
    start = TARGET_SPEED + index * 8;
    ts = ld2450::decode_speed(this->buffer_data_[start], this->buffer_data_[start + 1]);
    if (ts) {
      is_moving = true;
      moving_target_count++;
    }
#ifdef USE_SENSOR
    SAFE_PUBLISH_SENSOR(this->move_speed_sensors_[index], ts);
#endif
    // DISTANCE
    // Optimized: use already decoded tx and ty values, replace pow() with multiplication
    int32_t x_squared = (int32_t) tx * tx;
    int32_t y_squared = (int32_t) ty * ty;
    td = (uint16_t) sqrtf(x_squared + y_squared);
    if (td > 0) {
      target_count++;
    }
#ifdef USE_SENSOR
    SAFE_PUBLISH_SENSOR(this->move_distance_sensors_[index], td);
    // ANGLE
    angle = ld2450::calculate_angle(static_cast<float>(ty), static_cast<float>(td));
    if (tx > 0) {
      angle = angle * -1;
    }
    SAFE_PUBLISH_SENSOR(this->move_angle_sensors_[index], angle);
#endif
#ifdef USE_TEXT_SENSOR
    // DIRECTION
    if (td == 0) {
      direction = DIRECTION_NA;
    } else if (ts > 0) {
      direction = DIRECTION_MOVING_AWAY;
    } else if (ts < 0) {
      direction = DIRECTION_APPROACHING;
    } else {
      direction = DIRECTION_STATIONARY;
    }
    text_sensor::TextSensor *tsd = this->direction_text_sensors_[index];
    const auto *dir_str = find_str(ld2450::DIRECTION_BY_UINT, direction);
    if (tsd != nullptr && (!tsd->has_state() || tsd->get_state() != dir_str)) {
      tsd->publish_state(dir_str);
    }
#endif

    // Store target info for zone target count
    this->target_info_[index].x = tx;
    this->target_info_[index].y = ty;
    this->target_info_[index].is_moving = is_moving;

  }  // End loop thru targets

  still_target_count = target_count - moving_target_count;
#endif

#ifdef USE_SENSOR
  // Loop thru zones
  uint8_t zone_still_targets = 0;
  uint8_t zone_moving_targets = 0;
  uint8_t zone_all_targets = 0;
  for (index = 0; index < MAX_ZONES; index++) {
    zone_still_targets = this->count_targets_in_zone_(this->zone_config_[index], false);
    zone_moving_targets = this->count_targets_in_zone_(this->zone_config_[index], true);
    zone_all_targets = zone_still_targets + zone_moving_targets;

    // Publish Still Target Count in Zones
    SAFE_PUBLISH_SENSOR(this->zone_still_target_count_sensors_[index], zone_still_targets);
    // Publish Moving Target Count in Zones
    SAFE_PUBLISH_SENSOR(this->zone_moving_target_count_sensors_[index], zone_moving_targets);
    // Publish All Target Count in Zones
    SAFE_PUBLISH_SENSOR(this->zone_target_count_sensors_[index], zone_all_targets);
  }  // End loop thru zones

  // Target Count
  SAFE_PUBLISH_SENSOR(this->target_count_sensor_, target_count);
  // Still Target Count
  SAFE_PUBLISH_SENSOR(this->still_target_count_sensor_, still_target_count);
  // Moving Target Count
  SAFE_PUBLISH_SENSOR(this->moving_target_count_sensor_, moving_target_count);
#endif

#ifdef USE_BINARY_SENSOR
  // Target Presence
  if (this->target_binary_sensor_ != nullptr) {
    if (target_count > 0) {
      this->target_binary_sensor_->publish_state(true);
    } else {
      if (this->get_timeout_status_(this->presence_millis_)) {
        this->target_binary_sensor_->publish_state(false);
      } else {
        ESP_LOGV(TAG, "Clear presence waiting timeout: %d", this->timeout_);
      }
    }
  }
  // Moving Target Presence
  if (this->moving_target_binary_sensor_ != nullptr) {
    if (moving_target_count > 0) {
      this->moving_target_binary_sensor_->publish_state(true);
    } else {
      if (this->get_timeout_status_(this->moving_presence_millis_)) {
        this->moving_target_binary_sensor_->publish_state(false);
      }
    }
  }
  // Still Target Presence
  if (this->still_target_binary_sensor_ != nullptr) {
    if (still_target_count > 0) {
      this->still_target_binary_sensor_->publish_state(true);
    } else {
      if (this->get_timeout_status_(this->still_presence_millis_)) {
        this->still_target_binary_sensor_->publish_state(false);
      }
    }
  }
#endif
#ifdef USE_SENSOR
  // For presence timeout check
  if (target_count > 0) {
    this->presence_millis_ = App.get_loop_component_start_time();
  }
  if (moving_target_count > 0) {
    this->moving_presence_millis_ = App.get_loop_component_start_time();
  }
  if (still_target_count > 0) {
    this->still_presence_millis_ = App.get_loop_component_start_time();
  }
#endif
}

bool LD2450Component::handle_ack_data_() {
  ESP_LOGV(TAG, "Handling ACK DATA for COMMAND %02X", this->buffer_data_[COMMAND]);
  if (this->buffer_pos_ < 10) {
    ESP_LOGE(TAG, "Invalid length");
    return true;
  }
  if (!ld2450::validate_header_footer(CMD_FRAME_HEADER, this->buffer_data_)) {
    ESP_LOGW(TAG, "Invalid header: %s", format_hex_pretty(this->buffer_data_, HEADER_FOOTER_SIZE).c_str());
    return true;
  }
  if (this->buffer_data_[COMMAND_STATUS] != 0x01) {
    ESP_LOGE(TAG, "Invalid status");
    return true;
  }
  if (this->buffer_data_[8] || this->buffer_data_[9]) {
    ESP_LOGW(TAG, "Invalid command: %02X, %02X", this->buffer_data_[8], this->buffer_data_[9]);
    return true;
  }

  switch (this->buffer_data_[COMMAND]) {
    case CMD_ENABLE_CONF:
      ESP_LOGV(TAG, "Enable conf");
      break;

    case CMD_DISABLE_CONF:
      ESP_LOGV(TAG, "Disabled conf");
      break;

    case CMD_SET_BAUD_RATE:
      ESP_LOGV(TAG, "Baud rate change");
#ifdef USE_SELECT
      if (this->baud_rate_select_ != nullptr) {
        ESP_LOGE(TAG, "Change baud rate to %s and reinstall", this->baud_rate_select_->current_option());
      }
#endif
      break;

    case CMD_QUERY_VERSION: {
      std::memcpy(this->version_, &this->buffer_data_[12], sizeof(this->version_));
      char version_s[20];
      ld24xx::format_version_str(this->version_, version_s);
      ESP_LOGV(TAG, "Firmware version: %s", version_s);
#ifdef USE_TEXT_SENSOR
      if (this->version_text_sensor_ != nullptr) {
        this->version_text_sensor_->publish_state(version_s);
      }
#endif
      break;
    }

    case CMD_QUERY_MAC_ADDRESS: {
      if (this->buffer_pos_ < 20) {
        return false;
      }

      this->bluetooth_on_ = std::memcmp(&this->buffer_data_[10], NO_MAC, sizeof(NO_MAC)) != 0;
      if (this->bluetooth_on_) {
        std::memcpy(this->mac_address_, &this->buffer_data_[10], sizeof(this->mac_address_));
      }

      char mac_s[18];
      const char *mac_str = ld24xx::format_mac_str(this->mac_address_, mac_s);
      ESP_LOGV(TAG, "MAC address: %s", mac_str);
#ifdef USE_TEXT_SENSOR
      if (this->mac_text_sensor_ != nullptr) {
        this->mac_text_sensor_->publish_state(mac_str);
      }
#endif
#ifdef USE_SWITCH
      if (this->bluetooth_switch_ != nullptr) {
        this->bluetooth_switch_->publish_state(this->bluetooth_on_);
      }
#endif
      break;
    }

    case CMD_BLUETOOTH:
      ESP_LOGV(TAG, "Bluetooth");
      break;

    case CMD_SINGLE_TARGET_MODE:
      ESP_LOGV(TAG, "Single target conf");
#ifdef USE_SWITCH
      if (this->multi_target_switch_ != nullptr) {
        this->multi_target_switch_->publish_state(false);
      }
#endif
      break;

    case CMD_MULTI_TARGET_MODE:
      ESP_LOGV(TAG, "Multi target conf");
#ifdef USE_SWITCH
      if (this->multi_target_switch_ != nullptr) {
        this->multi_target_switch_->publish_state(true);
      }
#endif
      break;

    case CMD_QUERY_TARGET_MODE:
      ESP_LOGV(TAG, "Query target tracking mode");
#ifdef USE_SWITCH
      if (this->multi_target_switch_ != nullptr) {
        this->multi_target_switch_->publish_state(this->buffer_data_[10] == 0x02);
      }
#endif
      break;

    case CMD_QUERY_ZONE:
      ESP_LOGV(TAG, "Query zone conf");
      this->zone_type_ = std::stoi(std::to_string(this->buffer_data_[10]), nullptr, 16);
      this->publish_zone_type();
#ifdef USE_SELECT
      if (this->zone_type_select_ != nullptr) {
        ESP_LOGV(TAG, "Change zone type to: %s", this->zone_type_select_->current_option());
      }
#endif
      if (this->buffer_data_[10] == 0x00) {
        ESP_LOGV(TAG, "Zone: Disabled");
      }
      if (this->buffer_data_[10] == 0x01) {
        ESP_LOGV(TAG, "Zone: Area detection");
      }
      if (this->buffer_data_[10] == 0x02) {
        ESP_LOGV(TAG, "Zone: Area filter");
      }
      this->process_zone_();
      break;

    case CMD_SET_ZONE:
      ESP_LOGV(TAG, "Set zone conf");
      this->query_zone_info();
      break;

    default:
      break;
  }
  return true;
}

// Read LD2450 buffer data
void LD2450Component::readline_(int readch) {
  if (readch < 0) {
    return;  // No data available
  }

  if (this->buffer_pos_ < MAX_LINE_LENGTH - 1) {
    this->buffer_data_[this->buffer_pos_++] = readch;
    this->buffer_data_[this->buffer_pos_] = 0;
  } else {
    // We should never get here, but just in case...
    ESP_LOGW(TAG, "Max command length exceeded; ignoring");
    this->buffer_pos_ = 0;
  }
  if (this->buffer_pos_ < 4) {
    return;  // Not enough data to process yet
  }
  if (this->buffer_data_[this->buffer_pos_ - 2] == DATA_FRAME_FOOTER[0] &&
      this->buffer_data_[this->buffer_pos_ - 1] == DATA_FRAME_FOOTER[1]) {
    ESP_LOGV(TAG, "Handling Periodic Data: %s", format_hex_pretty(this->buffer_data_, this->buffer_pos_).c_str());
    this->handle_periodic_data_();
    this->buffer_pos_ = 0;  // Reset position index for next frame
  } else if (ld2450::validate_header_footer(CMD_FRAME_FOOTER, &this->buffer_data_[this->buffer_pos_ - 4])) {
    ESP_LOGV(TAG, "Handling Ack Data: %s", format_hex_pretty(this->buffer_data_, this->buffer_pos_).c_str());
    if (this->handle_ack_data_()) {
      this->buffer_pos_ = 0;  // Reset position index for next message
    } else {
      ESP_LOGV(TAG, "Ack Data incomplete");
    }
  }
}

// Set Config Mode - Pre-requisite sending commands
void LD2450Component::set_config_mode_(bool enable) {
  const uint8_t cmd = enable ? CMD_ENABLE_CONF : CMD_DISABLE_CONF;
  const uint8_t cmd_value[2] = {0x01, 0x00};
  this->send_command_(cmd, enable ? cmd_value : nullptr, sizeof(cmd_value));
}

// Set Bluetooth Enable/Disable
void LD2450Component::set_bluetooth(bool enable) {
  this->set_config_mode_(true);
  const uint8_t cmd_value[2] = {enable ? (uint8_t) 0x01 : (uint8_t) 0x00, 0x00};
  this->send_command_(CMD_BLUETOOTH, cmd_value, sizeof(cmd_value));
  this->set_timeout(200, [this]() { this->restart_and_read_all_info(); });
}

// Set Baud rate
void LD2450Component::set_baud_rate(const char *state) {
  this->set_config_mode_(true);
  const uint8_t cmd_value[2] = {find_uint8(BAUD_RATES_BY_STR, state), 0x00};
  this->send_command_(CMD_SET_BAUD_RATE, cmd_value, sizeof(cmd_value));
  this->set_timeout(200, [this]() { this->restart_(); });
}

// Set Zone Type - one of: Disabled, Detection, Filter
void LD2450Component::set_zone_type(const char *state) {
  ESP_LOGV(TAG, "Set zone type: %s", state);
  uint8_t zone_type = find_uint8(ZONE_TYPE_BY_STR, state);
  this->zone_type_ = zone_type;
  this->send_set_zone_command_();
}

// Publish Zone Type to Select component
void LD2450Component::publish_zone_type() {
#ifdef USE_SELECT
  std::string zone_type = find_str(ZONE_TYPE_BY_UINT, this->zone_type_);
  if (this->zone_type_select_ != nullptr) {
    this->zone_type_select_->publish_state(zone_type);
  }
#endif
}

// Set Single/Multiplayer target detection
void LD2450Component::set_multi_target(bool enable) {
  this->set_config_mode_(true);
  uint8_t cmd = enable ? CMD_MULTI_TARGET_MODE : CMD_SINGLE_TARGET_MODE;
  this->send_command_(cmd, nullptr, 0);
  this->set_config_mode_(false);
}

// LD2450 factory reset
void LD2450Component::factory_reset() {
  this->set_config_mode_(true);
  this->send_command_(CMD_RESET, nullptr, 0);
  this->set_timeout(200, [this]() { this->restart_and_read_all_info(); });
}

// Restart LD2450 module
void LD2450Component::restart_() { this->send_command_(CMD_RESTART, nullptr, 0); }

// Get LD2450 firmware version
void LD2450Component::get_version_() { this->send_command_(CMD_QUERY_VERSION, nullptr, 0); }

// Get LD2450 mac address
void LD2450Component::get_mac_() {
  uint8_t cmd_value[2] = {0x01, 0x00};
  this->send_command_(CMD_QUERY_MAC_ADDRESS, cmd_value, 2);
}

// Query for target tracking mode
void LD2450Component::query_target_tracking_mode_() { this->send_command_(CMD_QUERY_TARGET_MODE, nullptr, 0); }

// Query for zone info
void LD2450Component::query_zone_() { this->send_command_(CMD_QUERY_ZONE, nullptr, 0); }

#ifdef USE_SENSOR
// These could leak memory, but they are only set once prior to 'setup()' and should never be used again.
void LD2450Component::set_move_x_sensor(uint8_t target, sensor::Sensor *s) {
  this->move_x_sensors_[target] = new SensorWithDedup<int16_t>(s);
}
void LD2450Component::set_move_y_sensor(uint8_t target, sensor::Sensor *s) {
  this->move_y_sensors_[target] = new SensorWithDedup<int16_t>(s);
}
void LD2450Component::set_move_speed_sensor(uint8_t target, sensor::Sensor *s) {
  this->move_speed_sensors_[target] = new SensorWithDedup<int16_t>(s);
}
void LD2450Component::set_move_angle_sensor(uint8_t target, sensor::Sensor *s) {
  this->move_angle_sensors_[target] = new SensorWithDedup<float>(s);
}
void LD2450Component::set_move_distance_sensor(uint8_t target, sensor::Sensor *s) {
  this->move_distance_sensors_[target] = new SensorWithDedup<uint16_t>(s);
}
void LD2450Component::set_move_resolution_sensor(uint8_t target, sensor::Sensor *s) {
  this->move_resolution_sensors_[target] = new SensorWithDedup<uint16_t>(s);
}
void LD2450Component::set_zone_target_count_sensor(uint8_t zone, sensor::Sensor *s) {
  this->zone_target_count_sensors_[zone] = new SensorWithDedup<uint8_t>(s);
}
void LD2450Component::set_zone_still_target_count_sensor(uint8_t zone, sensor::Sensor *s) {
  this->zone_still_target_count_sensors_[zone] = new SensorWithDedup<uint8_t>(s);
}
void LD2450Component::set_zone_moving_target_count_sensor(uint8_t zone, sensor::Sensor *s) {
  this->zone_moving_target_count_sensors_[zone] = new SensorWithDedup<uint8_t>(s);
}
#endif
#ifdef USE_TEXT_SENSOR
void LD2450Component::set_direction_text_sensor(uint8_t target, text_sensor::TextSensor *s) {
  this->direction_text_sensors_[target] = s;
}
#endif

// Send Zone coordinates data to LD2450
#ifdef USE_NUMBER
void LD2450Component::set_zone_coordinate(uint8_t zone) {
  number::Number *x1sens = this->zone_numbers_[zone].x1;
  number::Number *y1sens = this->zone_numbers_[zone].y1;
  number::Number *x2sens = this->zone_numbers_[zone].x2;
  number::Number *y2sens = this->zone_numbers_[zone].y2;
  if (!x1sens->has_state() || !y1sens->has_state() || !x2sens->has_state() || !y2sens->has_state()) {
    return;
  }
  this->zone_config_[zone].x1 = static_cast<int>(x1sens->state);
  this->zone_config_[zone].y1 = static_cast<int>(y1sens->state);
  this->zone_config_[zone].x2 = static_cast<int>(x2sens->state);
  this->zone_config_[zone].y2 = static_cast<int>(y2sens->state);
  this->send_set_zone_command_();
}

void LD2450Component::set_zone_numbers(uint8_t zone, number::Number *x1, number::Number *y1, number::Number *x2,
                                       number::Number *y2) {
  if (zone < MAX_ZONES) {
    this->zone_numbers_[zone].x1 = x1;
    this->zone_numbers_[zone].y1 = y1;
    this->zone_numbers_[zone].x2 = x2;
    this->zone_numbers_[zone].y2 = y2;
  }
}
#endif

// Set Presence Timeout load and save from flash
#ifdef USE_NUMBER
void LD2450Component::set_presence_timeout() {
  if (this->presence_timeout_number_ != nullptr) {
    if (this->presence_timeout_number_->state == 0) {
      float timeout = this->restore_from_flash_();
      this->presence_timeout_number_->publish_state(timeout);
      this->timeout_ = ld2450::convert_seconds_to_ms(timeout);
    }
    if (this->presence_timeout_number_->has_state()) {
      this->save_to_flash_(this->presence_timeout_number_->state);
      this->timeout_ = ld2450::convert_seconds_to_ms(this->presence_timeout_number_->state);
    }
  }
}

// Save Presence Timeout to flash
void LD2450Component::save_to_flash_(float value) { this->pref_.save(&value); }

// Load Presence Timeout from flash
float LD2450Component::restore_from_flash_() {
  float value;
  if (!this->pref_.load(&value)) {
    value = DEFAULT_PRESENCE_TIMEOUT;
  }
  return value;
}
#endif

}  // namespace esphome::ld2450

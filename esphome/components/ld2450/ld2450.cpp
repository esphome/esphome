#include "ld2450.h"
#include <utility>
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#include "esphome/core/component.h"

#define highbyte(val) (uint8_t)((val) >> 8)
#define lowbyte(val) (uint8_t)((val) &0xff)

namespace esphome {
namespace ld2450 {

static const char *const TAG = "ld2450";
static const char *const UNKNOWN_MAC("unknown");

// LD2450 UART Serial Commands
static const uint8_t CMD_ENABLE_CONF = 0x00FF;
static const uint8_t CMD_DISABLE_CONF = 0x00FE;
static const uint8_t CMD_VERSION = 0x00A0;
static const uint8_t CMD_MAC = 0x00A5;
static const uint8_t CMD_RESET = 0x00A2;
static const uint8_t CMD_RESTART = 0x00A3;
static const uint8_t CMD_BLUETOOTH = 0x00A4;
static const uint8_t CMD_SINGLE_TARGET_MODE = 0x0080;
static const uint8_t CMD_MULTI_TARGET_MODE = 0x0090;
static const uint8_t CMD_QUERY_TARGET_MODE = 0x0091;
static const uint8_t CMD_SET_BAUD_RATE = 0x00A1;
static const uint8_t CMD_QUERY_ZONE = 0x00C1;
static const uint8_t CMD_SET_ZONE = 0x00C2;

static inline uint16_t convert_seconds_to_ms(uint16_t value) { return value * 1000; };

static inline std::string convert_signed_int_to_hex(int value) {
  auto value_as_str = str_snprintf("%04x", 4, value & 0xFFFF);
  return value_as_str;
}

static inline void convert_int_values_to_hex(const int *values, uint8_t *bytes) {
  for (int i = 0; i < 4; i++) {
    std::string temp_hex = convert_signed_int_to_hex(values[i]);
    bytes[i * 2] = std::stoi(temp_hex.substr(2, 2), nullptr, 16);      // Store high byte
    bytes[i * 2 + 1] = std::stoi(temp_hex.substr(0, 2), nullptr, 16);  // Store low byte
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
  if (base < 0.0 || hypotenuse <= 0.0) {
    return 0.0;
  }
  float angle_radians = std::acos(base / hypotenuse);
  float angle_degrees = angle_radians * (180.0 / M_PI);
  return angle_degrees;
}

static inline std::string get_direction(int16_t speed) {
  static const char *const APPROACHING = "Approaching";
  static const char *const MOVING_AWAY = "Moving away";
  static const char *const STATIONARY = "Stationary";

  if (speed > 0) {
    return MOVING_AWAY;
  }
  if (speed < 0) {
    return APPROACHING;
  }
  return STATIONARY;
}

static inline std::string format_mac(uint8_t *buffer) {
  return str_snprintf("%02X:%02X:%02X:%02X:%02X:%02X", 17, buffer[10], buffer[11], buffer[12], buffer[13], buffer[14],
                      buffer[15]);
}

static inline std::string format_version(uint8_t *buffer) {
  return str_sprintf("%u.%02X.%02X%02X%02X%02X", buffer[13], buffer[12], buffer[17], buffer[16], buffer[15],
                     buffer[14]);
}

LD2450Component::LD2450Component() {}

void LD2450Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up HLK-LD2450...");
#ifdef USE_NUMBER
  this->pref_ = global_preferences->make_preference<float>(this->presence_timeout_number_->get_object_id_hash());
  this->set_presence_timeout();
#endif
  this->read_all_info();
}

void LD2450Component::dump_config() {
  ESP_LOGCONFIG(TAG, "HLK-LD2450 Human motion tracking radar module:");
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "TargetBinarySensor", this->target_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "MovingTargetBinarySensor", this->moving_target_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "StillTargetBinarySensor", this->still_target_binary_sensor_);
#endif
#ifdef USE_SWITCH
  LOG_SWITCH("  ", "BluetoothSwitch", this->bluetooth_switch_);
  LOG_SWITCH("  ", "MultiTargetSwitch", this->multi_target_switch_);
#endif
#ifdef USE_BUTTON
  LOG_BUTTON("  ", "ResetButton", this->reset_button_);
  LOG_BUTTON("  ", "RestartButton", this->restart_button_);
#endif
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "TargetCountSensor", this->target_count_sensor_);
  LOG_SENSOR("  ", "StillTargetCountSensor", this->still_target_count_sensor_);
  LOG_SENSOR("  ", "MovingTargetCountSensor", this->moving_target_count_sensor_);
  for (sensor::Sensor *s : this->move_x_sensors_) {
    LOG_SENSOR("  ", "NthTargetXSensor", s);
  }
  for (sensor::Sensor *s : this->move_y_sensors_) {
    LOG_SENSOR("  ", "NthTargetYSensor", s);
  }
  for (sensor::Sensor *s : this->move_speed_sensors_) {
    LOG_SENSOR("  ", "NthTargetSpeedSensor", s);
  }
  for (sensor::Sensor *s : this->move_angle_sensors_) {
    LOG_SENSOR("  ", "NthTargetAngleSensor", s);
  }
  for (sensor::Sensor *s : this->move_distance_sensors_) {
    LOG_SENSOR("  ", "NthTargetDistanceSensor", s);
  }
  for (sensor::Sensor *s : this->move_resolution_sensors_) {
    LOG_SENSOR("  ", "NthTargetResolutionSensor", s);
  }
  for (sensor::Sensor *s : this->zone_target_count_sensors_) {
    LOG_SENSOR("  ", "NthZoneTargetCountSensor", s);
  }
  for (sensor::Sensor *s : this->zone_still_target_count_sensors_) {
    LOG_SENSOR("  ", "NthZoneStillTargetCountSensor", s);
  }
  for (sensor::Sensor *s : this->zone_moving_target_count_sensors_) {
    LOG_SENSOR("  ", "NthZoneMovingTargetCountSensor", s);
  }
#endif
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "VersionTextSensor", this->version_text_sensor_);
  LOG_TEXT_SENSOR("  ", "MacTextSensor", this->mac_text_sensor_);
  for (text_sensor::TextSensor *s : this->direction_text_sensors_) {
    LOG_TEXT_SENSOR("  ", "NthDirectionTextSensor", s);
  }
#endif
#ifdef USE_NUMBER
  for (number::Number *n : this->zone_x1_numbers_) {
    LOG_NUMBER("  ", "ZoneX1Number", n);
  }
  for (number::Number *n : this->zone_y1_numbers_) {
    LOG_NUMBER("  ", "ZoneY1Number", n);
  }
  for (number::Number *n : this->zone_x2_numbers_) {
    LOG_NUMBER("  ", "ZoneX2Number", n);
  }
  for (number::Number *n : this->zone_y2_numbers_) {
    LOG_NUMBER("  ", "ZoneY2Number", n);
  }
#endif
#ifdef USE_SELECT
  LOG_SELECT("  ", "BaudRateSelect", this->baud_rate_select_);
  LOG_SELECT("  ", "ZoneTypeSelect", this->zone_type_select_);
#endif
#ifdef USE_NUMBER
  LOG_NUMBER("  ", "PresenceTimeoutNumber", this->presence_timeout_number_);
#endif
  ESP_LOGCONFIG(TAG, "  Throttle : %ums", this->throttle_);
  ESP_LOGCONFIG(TAG, "  MAC Address : %s", const_cast<char *>(this->mac_.c_str()));
  ESP_LOGCONFIG(TAG, "  Firmware version : %s", const_cast<char *>(this->version_.c_str()));
}

void LD2450Component::loop() {
  while (this->available()) {
    this->readline_(read(), this->buffer_data_, MAX_LINE_LENGTH);
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
  for (int i = 0; i < MAX_ZONES; i++) {
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
  for (int i = 0; i < MAX_ZONES; i++) {
    int values[4] = {this->zone_config_[i].x1, this->zone_config_[i].y1, this->zone_config_[i].x2,
                     this->zone_config_[i].y2};
    ld2450::convert_int_values_to_hex(values, area_config + (i * 8));
  }
  std::memcpy(cmd_value, zone_type_bytes, 2);
  std::memcpy(cmd_value + 2, area_config, 24);
  this->set_config_mode_(true);
  this->send_command_(CMD_SET_ZONE, cmd_value, 26);
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
  auto current_millis = millis();
  return current_millis - check_millis >= this->timeout_;
}

// Extract, store and publish zone details LD2450 buffer
void LD2450Component::process_zone_(uint8_t *buffer) {
  uint8_t index, start;
  for (index = 0; index < MAX_ZONES; index++) {
    start = 12 + index * 8;
    this->zone_config_[index].x1 = ld2450::hex_to_signed_int(buffer, start);
    this->zone_config_[index].y1 = ld2450::hex_to_signed_int(buffer, start + 2);
    this->zone_config_[index].x2 = ld2450::hex_to_signed_int(buffer, start + 4);
    this->zone_config_[index].y2 = ld2450::hex_to_signed_int(buffer, start + 6);
#ifdef USE_NUMBER
    this->zone_x1_numbers_[index]->publish_state(this->zone_config_[index].x1);
    this->zone_y1_numbers_[index]->publish_state(this->zone_config_[index].y1);
    this->zone_x2_numbers_[index]->publish_state(this->zone_config_[index].x2);
    this->zone_y2_numbers_[index]->publish_state(this->zone_config_[index].y2);
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
  if (this->baud_rate_select_ != nullptr && this->baud_rate_select_->state != baud_rate) {
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
  this->set_timeout(1000, [this]() { this->read_all_info(); });
}

// Send command with values to LD2450
void LD2450Component::send_command_(uint8_t command, const uint8_t *command_value, uint8_t command_value_len) {
  ESP_LOGV(TAG, "Sending command %02X", command);
  // frame header
  this->write_array(CMD_FRAME_HEADER, 4);
  // length bytes
  int len = 2;
  if (command_value != nullptr) {
    len += command_value_len;
  }
  this->write_byte(lowbyte(len));
  this->write_byte(highbyte(len));
  // command
  this->write_byte(lowbyte(command));
  this->write_byte(highbyte(command));
  // command value bytes
  if (command_value != nullptr) {
    for (int i = 0; i < command_value_len; i++) {
      this->write_byte(command_value[i]);
    }
  }
  // footer
  this->write_array(CMD_FRAME_END, 4);
  // FIXME to remove
  delay(50);  // NOLINT
}

// LD2450 Radar data message:
//  [AA FF 03 00] [0E 03 B1 86 10 00 40 01] [00 00 00 00 00 00 00 00] [00 00 00 00 00 00 00 00] [55 CC]
//   Header       Target 1                  Target 2                  Target 3                  End
void LD2450Component::handle_periodic_data_(uint8_t *buffer, uint8_t len) {
  if (len < 29) {  // header (4 bytes) + 8 x 3 target data + footer (2 bytes)
    ESP_LOGE(TAG, "Periodic data: invalid message length");
    return;
  }
  if (buffer[0] != 0xAA || buffer[1] != 0xFF || buffer[2] != 0x03 || buffer[3] != 0x00) {  // header
    ESP_LOGE(TAG, "Periodic data: invalid message header");
    return;
  }
  if (buffer[len - 2] != 0x55 || buffer[len - 1] != 0xCC) {  // footer
    ESP_LOGE(TAG, "Periodic data: invalid message footer");
    return;
  }

  auto current_millis = millis();
  if (current_millis - this->last_periodic_millis_ < this->throttle_) {
    ESP_LOGV(TAG, "Throttling: %d", this->throttle_);
    return;
  }

  this->last_periodic_millis_ = current_millis;

  int16_t target_count = 0;
  int16_t still_target_count = 0;
  int16_t moving_target_count = 0;
  int16_t start = 0;
  int16_t val = 0;
  uint8_t index = 0;
  int16_t tx = 0;
  int16_t ty = 0;
  int16_t td = 0;
  int16_t ts = 0;
  int16_t angle = 0;
  std::string direction{};
  bool is_moving = false;

#ifdef USE_SENSOR
  // Loop thru targets
  // X
  for (index = 0; index < MAX_TARGETS; index++) {
    start = TARGET_X + index * 8;
    is_moving = false;
    sensor::Sensor *sx = this->move_x_sensors_[index];
    if (sx != nullptr) {
      val = ld2450::decode_coordinate(buffer[start], buffer[start + 1]);
      tx = val;
      sx->publish_state(val);
    }
    // Y
    start = TARGET_Y + index * 8;
    sensor::Sensor *sy = this->move_y_sensors_[index];
    if (sy != nullptr) {
      val = ld2450::decode_coordinate(buffer[start], buffer[start + 1]);
      ty = val;
      sy->publish_state(val);
    }
    // SPEED
    start = TARGET_SPEED + index * 8;
    sensor::Sensor *ss = this->move_speed_sensors_[index];
    if (ss != nullptr) {
      val = ld2450::decode_speed(buffer[start], buffer[start + 1]);
      ts = val;
      if (val) {
        is_moving = true;
        moving_target_count++;
      }
      ss->publish_state(val);
    }
    // RESOLUTION
    start = TARGET_RESOLUTION + index * 8;
    sensor::Sensor *sr = this->move_resolution_sensors_[index];
    if (sr != nullptr) {
      val = (buffer[start + 1] << 8) | buffer[start];
      sr->publish_state(val);
    }
    // DISTANCE
    sensor::Sensor *sd = this->move_distance_sensors_[index];
    if (sd != nullptr) {
      val = (uint16_t) sqrt(
          pow(ld2450::decode_coordinate(buffer[TARGET_X + index * 8], buffer[(TARGET_X + index * 8) + 1]), 2) +
          pow(ld2450::decode_coordinate(buffer[TARGET_Y + index * 8], buffer[(TARGET_Y + index * 8) + 1]), 2));
      td = val;
      if (val > 0) {
        target_count++;
      }

      sd->publish_state(val);
    }
    // ANGLE
    angle = calculate_angle(static_cast<float>(ty), static_cast<float>(td));
    if (tx > 0) {
      angle = angle * -1;
    }
    sensor::Sensor *sa = this->move_angle_sensors_[index];
    if (sa != nullptr) {
      sa->publish_state(angle);
    }
#endif
    // DIRECTION
#ifdef USE_TEXT_SENSOR
    direction = get_direction(ts);
    if (td == 0) {
      direction = "NA";
    }
    text_sensor::TextSensor *tsd = this->direction_text_sensors_[index];
    if (tsd != nullptr) {
      tsd->publish_state(direction);
    }
#endif

    // Store target info for zone target count
    this->target_info_[index].x = tx;
    this->target_info_[index].y = ty;
    this->target_info_[index].is_moving = is_moving;

  }  // End loop thru targets

#ifdef USE_SENSOR
  // Loop thru zones
  uint8_t zone_still_targets = 0;
  uint8_t zone_moving_targets = 0;
  uint8_t zone_all_targets = 0;
  for (index = 0; index < MAX_ZONES; index++) {
    // Publish Still Target Count in Zones
    sensor::Sensor *szstc = this->zone_still_target_count_sensors_[index];
    if (szstc != nullptr) {
      zone_still_targets = this->count_targets_in_zone_(this->zone_config_[index], false);
      szstc->publish_state(zone_still_targets);
    }
    // Publish Moving Target Count in Zones
    sensor::Sensor *szmtc = this->zone_moving_target_count_sensors_[index];
    if (szmtc != nullptr) {
      zone_moving_targets = this->count_targets_in_zone_(this->zone_config_[index], true);
      szmtc->publish_state(zone_moving_targets);
    }

    zone_all_targets = zone_still_targets + zone_moving_targets;

    // Publish All Target Count in Zones
    sensor::Sensor *sztc = this->zone_target_count_sensors_[index];
    if (sztc != nullptr) {
      sztc->publish_state(zone_all_targets);
    }

  }  // End loop thru zones

  still_target_count = target_count - moving_target_count;
  // Target Count
  if (this->target_count_sensor_ != nullptr) {
    this->target_count_sensor_->publish_state(target_count);
  }
  // Still Target Count
  if (this->still_target_count_sensor_ != nullptr) {
    this->still_target_count_sensor_->publish_state(still_target_count);
  }
  // Moving Target Count
  if (this->moving_target_count_sensor_ != nullptr) {
    this->moving_target_count_sensor_->publish_state(moving_target_count);
  }
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
    this->presence_millis_ = millis();
  }
  if (moving_target_count > 0) {
    this->moving_presence_millis_ = millis();
  }
  if (still_target_count > 0) {
    this->still_presence_millis_ = millis();
  }
#endif
}

bool LD2450Component::handle_ack_data_(uint8_t *buffer, uint8_t len) {
  ESP_LOGV(TAG, "Handling ack data for command %02X", buffer[COMMAND]);
  if (len < 10) {
    ESP_LOGE(TAG, "Ack data: invalid length");
    return true;
  }
  if (buffer[0] != 0xFD || buffer[1] != 0xFC || buffer[2] != 0xFB || buffer[3] != 0xFA) {  // frame header
    ESP_LOGE(TAG, "Ack data: invalid header (command %02X)", buffer[COMMAND]);
    return true;
  }
  if (buffer[COMMAND_STATUS] != 0x01) {
    ESP_LOGE(TAG, "Ack data: invalid status");
    return true;
  }
  if (buffer[8] || buffer[9]) {
    ESP_LOGE(TAG, "Ack data: last buffer was %u, %u", buffer[8], buffer[9]);
    return true;
  }

  switch (buffer[COMMAND]) {
    case lowbyte(CMD_ENABLE_CONF):
      ESP_LOGV(TAG, "Got enable conf command");
      break;
    case lowbyte(CMD_DISABLE_CONF):
      ESP_LOGV(TAG, "Got disable conf command");
      break;
    case lowbyte(CMD_SET_BAUD_RATE):
      ESP_LOGV(TAG, "Got baud rate change command");
#ifdef USE_SELECT
      if (this->baud_rate_select_ != nullptr) {
        ESP_LOGV(TAG, "Change baud rate to %s", this->baud_rate_select_->state.c_str());
      }
#endif
      break;
    case lowbyte(CMD_VERSION):
      this->version_ = ld2450::format_version(buffer);
      ESP_LOGV(TAG, "Firmware version: %s", this->version_.c_str());
#ifdef USE_TEXT_SENSOR
      if (this->version_text_sensor_ != nullptr) {
        this->version_text_sensor_->publish_state(this->version_);
      }
#endif
      break;
    case lowbyte(CMD_MAC):
      if (len < 20) {
        return false;
      }
      this->mac_ = ld2450::format_mac(buffer);
      ESP_LOGV(TAG, "MAC address: %s", this->mac_.c_str());
#ifdef USE_TEXT_SENSOR
      if (this->mac_text_sensor_ != nullptr) {
        this->mac_text_sensor_->publish_state(this->mac_);
      }
#endif
#ifdef USE_SWITCH
      if (this->bluetooth_switch_ != nullptr) {
        this->bluetooth_switch_->publish_state(this->mac_ != UNKNOWN_MAC);
      }
#endif
      break;
    case lowbyte(CMD_BLUETOOTH):
      ESP_LOGV(TAG, "Got Bluetooth command");
      break;
    case lowbyte(CMD_SINGLE_TARGET_MODE):
      ESP_LOGV(TAG, "Got single target conf command");
#ifdef USE_SWITCH
      if (this->multi_target_switch_ != nullptr) {
        this->multi_target_switch_->publish_state(false);
      }
#endif
      break;
    case lowbyte(CMD_MULTI_TARGET_MODE):
      ESP_LOGV(TAG, "Got multi target conf command");
#ifdef USE_SWITCH
      if (this->multi_target_switch_ != nullptr) {
        this->multi_target_switch_->publish_state(true);
      }
#endif
      break;
    case lowbyte(CMD_QUERY_TARGET_MODE):
      ESP_LOGV(TAG, "Got query target tracking mode command");
#ifdef USE_SWITCH
      if (this->multi_target_switch_ != nullptr) {
        this->multi_target_switch_->publish_state(buffer[10] == 0x02);
      }
#endif
      break;
    case lowbyte(CMD_QUERY_ZONE):
      ESP_LOGV(TAG, "Got query zone conf command");
      this->zone_type_ = std::stoi(std::to_string(buffer[10]), nullptr, 16);
      this->publish_zone_type();
#ifdef USE_SELECT
      if (this->zone_type_select_ != nullptr) {
        ESP_LOGV(TAG, "Change zone type to: %s", this->zone_type_select_->state.c_str());
      }
#endif
      if (buffer[10] == 0x00) {
        ESP_LOGV(TAG, "Zone: Disabled");
      }
      if (buffer[10] == 0x01) {
        ESP_LOGV(TAG, "Zone: Area detection");
      }
      if (buffer[10] == 0x02) {
        ESP_LOGV(TAG, "Zone: Area filter");
      }
      this->process_zone_(buffer);
      break;
    case lowbyte(CMD_SET_ZONE):
      ESP_LOGV(TAG, "Got set zone conf command");
      this->query_zone_info();
      break;
    default:
      break;
  }
  return true;
}

// Read LD2450 buffer data
void LD2450Component::readline_(int readch, uint8_t *buffer, uint8_t len) {
  if (readch < 0) {
    return;
  }
  if (this->buffer_pos_ < len - 1) {
    buffer[this->buffer_pos_++] = readch;
    buffer[this->buffer_pos_] = 0;
  } else {
    this->buffer_pos_ = 0;
  }
  if (this->buffer_pos_ < 4) {
    return;
  }
  if (buffer[this->buffer_pos_ - 2] == 0x55 && buffer[this->buffer_pos_ - 1] == 0xCC) {
    ESP_LOGV(TAG, "Handle periodic radar data");
    this->handle_periodic_data_(buffer, this->buffer_pos_);
    this->buffer_pos_ = 0;  // Reset position index for next frame
  } else if (buffer[this->buffer_pos_ - 4] == 0x04 && buffer[this->buffer_pos_ - 3] == 0x03 &&
             buffer[this->buffer_pos_ - 2] == 0x02 && buffer[this->buffer_pos_ - 1] == 0x01) {
    ESP_LOGV(TAG, "Handle command ack data");
    if (this->handle_ack_data_(buffer, this->buffer_pos_)) {
      this->buffer_pos_ = 0;  // Reset position index for next frame
    } else {
      ESP_LOGV(TAG, "Command ack data invalid");
    }
  }
}

// Set Config Mode - Pre-requisite sending commands
void LD2450Component::set_config_mode_(bool enable) {
  uint8_t cmd = enable ? CMD_ENABLE_CONF : CMD_DISABLE_CONF;
  uint8_t cmd_value[2] = {0x01, 0x00};
  this->send_command_(cmd, enable ? cmd_value : nullptr, 2);
}

// Set Bluetooth Enable/Disable
void LD2450Component::set_bluetooth(bool enable) {
  this->set_config_mode_(true);
  uint8_t enable_cmd_value[2] = {0x01, 0x00};
  uint8_t disable_cmd_value[2] = {0x00, 0x00};
  this->send_command_(CMD_BLUETOOTH, enable ? enable_cmd_value : disable_cmd_value, 2);
  this->set_timeout(200, [this]() { this->restart_and_read_all_info(); });
}

// Set Baud rate
void LD2450Component::set_baud_rate(const std::string &state) {
  this->set_config_mode_(true);
  uint8_t cmd_value[2] = {BAUD_RATE_ENUM_TO_INT.at(state), 0x00};
  this->send_command_(CMD_SET_BAUD_RATE, cmd_value, 2);
  this->set_timeout(200, [this]() { this->restart_(); });
}

// Set Zone Type - one of: Disabled, Detection, Filter
void LD2450Component::set_zone_type(const std::string &state) {
  ESP_LOGV(TAG, "Set zone type: %s", state.c_str());
  uint8_t zone_type = ZONE_TYPE_ENUM_TO_INT.at(state);
  this->zone_type_ = zone_type;
  this->send_set_zone_command_();
}

// Publish Zone Type to Select component
void LD2450Component::publish_zone_type() {
#ifdef USE_SELECT
  std::string zone_type = ZONE_TYPE_INT_TO_ENUM.at(static_cast<ZoneTypeStructure>(this->zone_type_));
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
void LD2450Component::get_version_() { this->send_command_(CMD_VERSION, nullptr, 0); }

// Get LD2450 mac address
void LD2450Component::get_mac_() {
  uint8_t cmd_value[2] = {0x01, 0x00};
  this->send_command_(CMD_MAC, cmd_value, 2);
}

// Query for target tracking mode
void LD2450Component::query_target_tracking_mode_() { this->send_command_(CMD_QUERY_TARGET_MODE, nullptr, 0); }

// Query for zone info
void LD2450Component::query_zone_() { this->send_command_(CMD_QUERY_ZONE, nullptr, 0); }

#ifdef USE_SENSOR
void LD2450Component::set_move_x_sensor(uint8_t target, sensor::Sensor *s) { this->move_x_sensors_[target] = s; }
void LD2450Component::set_move_y_sensor(uint8_t target, sensor::Sensor *s) { this->move_y_sensors_[target] = s; }
void LD2450Component::set_move_speed_sensor(uint8_t target, sensor::Sensor *s) {
  this->move_speed_sensors_[target] = s;
}
void LD2450Component::set_move_angle_sensor(uint8_t target, sensor::Sensor *s) {
  this->move_angle_sensors_[target] = s;
}
void LD2450Component::set_move_distance_sensor(uint8_t target, sensor::Sensor *s) {
  this->move_distance_sensors_[target] = s;
}
void LD2450Component::set_move_resolution_sensor(uint8_t target, sensor::Sensor *s) {
  this->move_resolution_sensors_[target] = s;
}
void LD2450Component::set_zone_target_count_sensor(uint8_t zone, sensor::Sensor *s) {
  this->zone_target_count_sensors_[zone] = s;
}
void LD2450Component::set_zone_still_target_count_sensor(uint8_t zone, sensor::Sensor *s) {
  this->zone_still_target_count_sensors_[zone] = s;
}
void LD2450Component::set_zone_moving_target_count_sensor(uint8_t zone, sensor::Sensor *s) {
  this->zone_moving_target_count_sensors_[zone] = s;
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
  number::Number *x1sens = this->zone_x1_numbers_[zone];
  number::Number *y1sens = this->zone_y1_numbers_[zone];
  number::Number *x2sens = this->zone_x2_numbers_[zone];
  number::Number *y2sens = this->zone_y2_numbers_[zone];
  if (!x1sens->has_state() || !y1sens->has_state() || !x2sens->has_state() || !y2sens->has_state()) {
    return;
  }
  this->zone_config_[zone].x1 = static_cast<int>(x1sens->state);
  this->zone_config_[zone].y1 = static_cast<int>(y1sens->state);
  this->zone_config_[zone].x2 = static_cast<int>(x2sens->state);
  this->zone_config_[zone].y2 = static_cast<int>(y2sens->state);
  this->send_set_zone_command_();
}

void LD2450Component::set_zone_x1_number(uint8_t zone, number::Number *n) { this->zone_x1_numbers_[zone] = n; }
void LD2450Component::set_zone_y1_number(uint8_t zone, number::Number *n) { this->zone_y1_numbers_[zone] = n; }
void LD2450Component::set_zone_x2_number(uint8_t zone, number::Number *n) { this->zone_x2_numbers_[zone] = n; }
void LD2450Component::set_zone_y2_number(uint8_t zone, number::Number *n) { this->zone_y2_numbers_[zone] = n; }
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

}  // namespace ld2450
}  // namespace esphome

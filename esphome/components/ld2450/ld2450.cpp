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

LD2450Component::LD2450Component() {}

void LD2450Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up HLK-LD2450");
#ifdef USE_NUMBER
  this->pref_ = global_preferences->make_preference<float>(this->presence_timeout_number_->get_object_id_hash());
  this->set_presence_timeout();
#endif
  this->read_all_info();
  ESP_LOGCONFIG(TAG, "Mac Address : %s", const_cast<char *>(this->mac_.c_str()));
  ESP_LOGCONFIG(TAG, "Firmware Version : %s", const_cast<char *>(this->version_.c_str()));
  ESP_LOGCONFIG(TAG, "HLK-LD2450 setup complete");
#ifdef USE_API
  ESP_LOGCONFIG(TAG, "Registering services");
  CustomAPIDevice::register_service(&::esphome::ld2450::LD2450Component::on_set_radar_zone_, "set_radar_zone",
                                    {
                                        "zone_type",
                                        "zone1_x1",
                                        "zone1_y1",
                                        "zone1_x2",
                                        "zone1_y2",
                                        "zone2_x1",
                                        "zone2_y1",
                                        "zone2_x2",
                                        "zone2_y2",
                                        "zone3_x1",
                                        "zone3_y1",
                                        "zone3_x2",
                                        "zone3_y2",
                                    });
  CustomAPIDevice::register_service(&::esphome::ld2450::LD2450Component::on_reset_radar_zone_, "reset_radar_zone");
  ESP_LOGCONFIG(TAG, "Services registration complete");
#endif
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
  this->read_all_info();
  ESP_LOGCONFIG(TAG, "  Throttle_ : %ums", this->throttle_);
  ESP_LOGCONFIG(TAG, "  MAC Address : %s", const_cast<char *>(this->mac_.c_str()));
  ESP_LOGCONFIG(TAG, "  Firmware Version : %s", const_cast<char *>(this->version_.c_str()));
}

void LD2450Component::loop() {
  const int max_line_length = 80;
  static uint8_t buffer[max_line_length];

  while (available()) {
    this->readline_(read(), buffer, max_line_length);
  }
}

// Service reset_radar_zone
void LD2450Component::on_reset_radar_zone_() {
  this->zone_type_ = 0;
  for (auto &i : zone_config_) {
    i.x1 = 0;
    i.y1 = 0;
    i.x2 = 0;
    i.y2 = 0;
  }
  this->send_set_zone_command_();
}

// Service set_radar_zone
void LD2450Component::on_set_radar_zone_(int zone_type, int zone1_x1, int zone1_y1, int zone1_x2, int zone1_y2,
                                         int zone2_x1, int zone2_y1, int zone2_x2, int zone2_y2, int zone3_x1,
                                         int zone3_y1, int zone3_x2, int zone3_y2) {
  this->zone_type_ = zone_type;
  int zone_parameters[12] = {zone1_x1, zone1_y1, zone1_x2, zone1_y2, zone2_x1, zone2_y1,
                             zone2_x2, zone2_y2, zone3_x1, zone3_y1, zone3_x2, zone3_y2};
  for (int i = 0; i < MAX_ZONES; i++) {
    zone_config_[i].x1 = zone_parameters[i * 4];
    zone_config_[i].y1 = zone_parameters[i * 4 + 1];
    zone_config_[i].x2 = zone_parameters[i * 4 + 2];
    zone_config_[i].y2 = zone_parameters[i * 4 + 3];
  }
  this->send_set_zone_command_();
}

// Set Zone on LD2450 Sensor
void LD2450Component::send_set_zone_command_() {
  uint8_t cmd_value[26] = {};
  uint8_t zone_type_bytes[2] = {static_cast<uint8_t>(this->zone_type_), 0x00};
  uint8_t area_config[24] = {};
  for (int i = 0; i < MAX_ZONES; i++) {
    int values[4] = {zone_config_[i].x1, zone_config_[i].y1, zone_config_[i].x2, zone_config_[i].y2};
    this->convert_int_values_to_hex_(values, area_config + (i * 8));
  }
  std::memcpy(cmd_value, zone_type_bytes, 2);
  std::memcpy(cmd_value + 2, area_config, 24);
  set_config_mode_(true);
  send_command_(CMD_SET_ZONE, cmd_value, 26);
  set_config_mode_(false);
}

// Convert signed int to HEX high and low bytes
void LD2450Component::convert_int_values_to_hex_(const int *values, uint8_t *bytes) {
  for (int i = 0; i < 4; i++) {
    std::string temp_hex = convert_signed_int_to_hex_(values[i]);
    bytes[i * 2] = std::stoi(temp_hex.substr(2, 2), nullptr, 16);      // Store high byte
    bytes[i * 2 + 1] = std::stoi(temp_hex.substr(0, 2), nullptr, 16);  // Store low byte
  }
}

// Check presense timeout to reset presence status
bool LD2450Component::get_timeout_status_(int32_t check_millis) {
  if (check_millis == 0)
    return true;
  if (this->timeout_ == 0)
    this->timeout_ = this->convert_seconds_to_ms(DEFAULT_PRESENCE_TIMEOUT);
  int32_t current_millis = millis();
  int32_t timeout = this->timeout_;
  return current_millis - check_millis >= timeout;
}

// Extract, store and publish zone details LD2450 buffer
void LD2450Component::process_zone_(uint8_t *buffer) {
  uint8_t index, start;
  for (index = 0; index < MAX_ZONES; index++) {
    start = 12 + index * 8;
    zone_config_[index].x1 = this->hex_to_signed_int_(buffer, start);
    zone_config_[index].y1 = this->hex_to_signed_int_(buffer, start + 2);
    zone_config_[index].x2 = this->hex_to_signed_int_(buffer, start + 4);
    zone_config_[index].y2 = this->hex_to_signed_int_(buffer, start + 6);
#ifdef USE_NUMBER
    this->zone_x1_numbers_[index]->publish_state(zone_config_[index].x1);
    this->zone_y1_numbers_[index]->publish_state(zone_config_[index].y1);
    this->zone_x2_numbers_[index]->publish_state(zone_config_[index].x2);
    this->zone_y2_numbers_[index]->publish_state(zone_config_[index].y2);
#endif
  }
}

// Read all info from LD2450 buffer
void LD2450Component::read_all_info() {
  this->set_config_mode_(true);
  this->get_version_();
  this->get_mac_();
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
void LD2450Component::send_command_(uint8_t command, const uint8_t *command_value, int command_value_len) {
  ESP_LOGV(TAG, "Sending COMMAND %02X", command);
  // frame start bytes
  this->write_array(CMD_FRAME_HEADER, 4);
  // length bytes
  int len = 2;
  if (command_value != nullptr)
    len += command_value_len;
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
  // frame end bytes
  this->write_array(CMD_FRAME_END, 4);
  // FIXME to remove
  delay(50);  // NOLINT
}

// LD2450 Radar data output protocol
// Eg: [AA FF 03 00] [0E 03 B1 86 10 00 40 01] [00 00 00 00 00 00 00 00] [00 00 00 00 00 00 00 00] [55 CC]
//      Header       Target 1                  Target 2                  Target 3                  End
void LD2450Component::handle_periodic_data_(uint8_t *buffer, int len) {
  if (len < 29)
    return;  // 4 frame start bytes + 8 x 3 Target Data  + 2 frame end bytes
  if (buffer[0] != 0xAA || buffer[1] != 0xFF || buffer[2] != 0x03 || buffer[3] != 0x00)  // check 4 frame start bytes
    return;
  if (buffer[len - 2] != 0x55 || buffer[len - 1] != 0xCC)  // Check 2 end frame bytes
    return;                                                // frame end=0x55 0xCC
  int32_t current_millis = millis();
  if (current_millis - uptime_millis_ < START_DELAY) {
    ESP_LOGV(TAG, "Waiting for Delayed Start: %d", START_DELAY);
    return;
  }
  if (current_millis - this->last_periodic_millis_ < this->throttle_) {
    ESP_LOGV(TAG, "Throttling: %d", this->throttle_);
    return;
  }

  this->last_periodic_millis_ = current_millis;

  int16_t target_count = 0;
  int16_t still_target_count = 0;
  int16_t moving_target_count = 0;
  int16_t start;
  int16_t val;
  uint8_t index;
  int16_t tx;
  int16_t ty;
  int16_t td;
  int16_t ts;
  int16_t angle;
  std::string direction;

#ifdef USE_SENSOR
  // Loop thru targets
  // X
  for (index = 0; index < MAX_TARGETS; index++) {
    start = TARGET_X + index * 8;
    sensor::Sensor *sx = this->move_x_sensors_[index];
    if (sx != nullptr) {
      val = this->decode_coordinate_(buffer[start], buffer[start + 1]);
      tx = val;
      if (sx->get_state() != val) {
        sx->publish_state(val);
      }
    }
    // Y
    start = TARGET_Y + index * 8;
    sensor::Sensor *sy = this->move_y_sensors_[index];
    if (sy != nullptr) {
      val = this->decode_coordinate_(buffer[start], buffer[start + 1]);
      ty = val;
      if (sy->get_state() != val) {
        sy->publish_state(val);
      }
    }
    // SPEED
    start = TARGET_SPEED + index * 8;
    sensor::Sensor *ss = this->move_speed_sensors_[index];
    if (ss != nullptr) {
      val = this->decode_speed_(buffer[start], buffer[start + 1]);
      ts = val;
      if (val > 0)
        moving_target_count++;
      if (ss->get_state() != val) {
        ss->publish_state(val);
      }
    }
    // RESOLUTION
    start = TARGET_RESOLUTION + index * 8;
    sensor::Sensor *sr = this->move_resolution_sensors_[index];
    if (sr != nullptr) {
      val = (buffer[start + 1] << 8) | buffer[start];
      if (sr->get_state() != val) {
        sr->publish_state(val);
      }
    }
    // DISTANCE
    sensor::Sensor *sd = this->move_distance_sensors_[index];
    if (sd != nullptr) {
      val = (uint16_t) sqrt(
          pow(this->decode_coordinate_(buffer[TARGET_X + index * 8], buffer[(TARGET_X + index * 8) + 1]), 2) +
          pow(this->decode_coordinate_(buffer[TARGET_Y + index * 8], buffer[(TARGET_Y + index * 8) + 1]), 2));
      td = val;
      if (val > 0)
        target_count++;

      if (sd->get_state() != val) {
        sd->publish_state(val);
      }
    }
    // ANGLE
    angle = calculate_angle_(static_cast<float>(ty), static_cast<float>(td));
    if (tx > 0) {
      angle = angle * -1;
    }
    sensor::Sensor *sa = this->move_angle_sensors_[index];
    if (sa != nullptr) {
      if (sa->get_state() != angle) {
        sa->publish_state(angle);
      }
    }
#endif
    // DIRECTION
#ifdef USE_TEXT_SENSOR
    direction = get_direction_(ts);
    if (td == 0)
      direction = "NA";
    text_sensor::TextSensor *tsd = this->direction_text_sensors_[index];
    if (tsd != nullptr) {
      if (tsd->get_state() != direction) {
        tsd->publish_state(direction);
      }
    }
#endif
  }  // End loop thru targets

#ifdef USE_SENSOR
  still_target_count = target_count - moving_target_count;
  // Target Count
  if (this->target_count_sensor_ != nullptr) {
    if (this->target_count_sensor_->get_state() != target_count) {
      this->target_count_sensor_->publish_state(target_count);
    }
  }
  // Still Target Count
  if (this->still_target_count_sensor_ != nullptr) {
    if (this->still_target_count_sensor_->get_state() != still_target_count) {
      this->still_target_count_sensor_->publish_state(still_target_count);
    }
  }
  // Moving Target Count
  if (this->moving_target_count_sensor_ != nullptr) {
    if (this->moving_target_count_sensor_->get_state() != moving_target_count) {
      this->moving_target_count_sensor_->publish_state(moving_target_count);
    }
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
        ESP_LOGV(TAG, "Clear Presence Waiting Timeout: %d", this->timeout_);
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

const char VERSION_FMT[] = "%u.%02X.%02X%02X%02X%02X";

std::string format_version(uint8_t *buffer) {
  std::string::size_type version_size = 256;
  std::string version;
  do {
    version.resize(version_size + 1);
    version_size = std::snprintf(&version[0], version.size(), VERSION_FMT, buffer[13], buffer[12], buffer[17],
                                 buffer[16], buffer[15], buffer[14]);
  } while (version_size + 1 > version.size());
  version.resize(version_size);
  return version;
}

const char MAC_FMT[] = "%02X:%02X:%02X:%02X:%02X:%02X";

const std::string UNKNOWN_MAC("unknown");
const std::string NO_MAC("08:05:04:03:02:01");

std::string format_mac(uint8_t *buffer) {
  std::string::size_type mac_size = 256;
  std::string mac;
  do {
    mac.resize(mac_size + 1);
    mac_size = std::snprintf(&mac[0], mac.size(), MAC_FMT, buffer[10], buffer[11], buffer[12], buffer[13], buffer[14],
                             buffer[15]);
  } while (mac_size + 1 > mac.size());
  mac.resize(mac_size);
  if (mac == NO_MAC)
    return UNKNOWN_MAC;
  return mac;
}

bool LD2450Component::handle_ack_data_(uint8_t *buffer, int len) {
  ESP_LOGD(TAG, "Handling ACK DATA for COMMAND %02X", buffer[COMMAND]);
  if (len < 10) {
    ESP_LOGE(TAG, "Error with last command: Incorrect length");
    return true;
  }
  if (buffer[0] != 0xFD || buffer[1] != 0xFC || buffer[2] != 0xFB || buffer[3] != 0xFA) {  // check 4 frame start bytes
    ESP_LOGE(TAG, "Error with last command: Incorrect Header. COMMAND: %02X", buffer[COMMAND]);
    return true;
  }
  if (buffer[COMMAND_STATUS] != 0x01) {
    ESP_LOGE(TAG, "Error with last command: Status != 0x01");
    return true;
  }
  if (this->two_byte_to_int_(buffer[8], buffer[9]) != 0x00) {
    ESP_LOGE(TAG, "Error with last command, last buffer was: %u , %u", buffer[8], buffer[9]);
    return true;
  }
  switch (buffer[COMMAND]) {
    case lowbyte(CMD_ENABLE_CONF):
      ESP_LOGV(TAG, "Handled Enable conf command");
      break;
    case lowbyte(CMD_DISABLE_CONF):
      ESP_LOGV(TAG, "Handled Disabled conf command");
      break;
    case lowbyte(CMD_SET_BAUD_RATE):
      ESP_LOGV(TAG, "Handled baud rate change command");
#ifdef USE_SELECT
      if (this->baud_rate_select_ != nullptr) {
        ESP_LOGV(TAG, "Change baud rate component config to %s and reinstall", this->baud_rate_select_->state.c_str());
      }
#endif
      break;
    case lowbyte(CMD_VERSION):
      this->version_ = format_version(buffer);
      ESP_LOGV(TAG, "LD2450 Firmware Version: %s", const_cast<char *>(this->version_.c_str()));
#ifdef USE_TEXT_SENSOR
      if (this->version_text_sensor_ != nullptr) {
        this->version_text_sensor_->publish_state(this->version_);
      }
#endif
      break;
    case lowbyte(CMD_MAC):
      if (len < 20)
        return false;
      this->mac_ = format_mac(buffer);
      ESP_LOGV(TAG, "LD2450 MAC Address: %s", const_cast<char *>(this->mac_.c_str()));
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
      ESP_LOGV(TAG, "Handled Bluetooth command");
      break;
    case lowbyte(CMD_SINGLE_TARGET):
      ESP_LOGV(TAG, "Handled Single Target conf command");
#ifdef USE_SWITCH
      if (this->multi_target_switch_ != nullptr) {
        this->multi_target_switch_->publish_state(false);
      }
#endif
      break;
    case lowbyte(CMD_MULTI_TARGET):
      ESP_LOGV(TAG, "Handled Multi Target conf command");
#ifdef USE_SWITCH
      if (this->multi_target_switch_ != nullptr) {
        this->multi_target_switch_->publish_state(true);
      }
#endif
      break;
    case lowbyte(CMD_QUERY_ZONE):
      ESP_LOGV(TAG, "Handled Query Zone conf command");
      this->zone_type_ = std::stoi(std::to_string(buffer[10]), nullptr, 16);
      this->publish_zone_type();
#ifdef USE_SELECT
      if (this->zone_type_select_ != nullptr) {
        ESP_LOGV(TAG, "Change Zone Type component config to: %s", this->zone_type_select_->state.c_str());
      }
#endif
      if (buffer[10] == 0x00) {
        ESP_LOGV(TAG, "Zone: Disabled");
      }
      if (buffer[10] == 0x01) {
        ESP_LOGV(TAG, "Zone: Area Detection");
      }
      if (buffer[10] == 0x02) {
        ESP_LOGV(TAG, "Zone: Area Filter");
      }
      this->process_zone_(buffer);
      break;
    case lowbyte(CMD_SET_ZONE):
      ESP_LOGV(TAG, "Handled SET Zone conf command");
      this->query_zone_info();
      break;
    default:
      break;
  }
  return true;
}

// Read LD2450 buffer data
void LD2450Component::readline_(int readch, uint8_t *buffer, int len) {
  static int pos = 0;
  if (readch >= 0) {
    if (pos < len - 1) {
      buffer[pos++] = readch;
      buffer[pos] = 0;
    } else {
      pos = 0;
    }
    if (pos >= 4) {
      if (buffer[pos - 2] == 0x55 && buffer[pos - 1] == 0xCC) {
        ESP_LOGV(TAG, "Handle Periodic Radar Data");
        this->handle_periodic_data_(buffer, pos);
        pos = 0;  // Reset position index ready for next time
      } else if (buffer[pos - 4] == 0x04 && buffer[pos - 3] == 0x03 && buffer[pos - 2] == 0x02 &&
                 buffer[pos - 1] == 0x01) {
        ESP_LOGV(TAG, "Handle Commad ACK Data");
        if (this->handle_ack_data_(buffer, pos)) {
          pos = 0;  // Reset position index ready for next time
        } else {
          ESP_LOGV(TAG, "Command ACK Data incomplete");
        }
      }
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
  std::string zone_type = ZONE_TYPE_INT_TO_ENUM.at(static_cast<ZoneTypeStructure>(this->zone_type_));
#ifdef USE_SELECT
  if (this->zone_type_select_ != nullptr && this->zone_type_select_->state != zone_type) {
    this->zone_type_select_->publish_state(zone_type);
  }
#endif
}

// Set Single/Multiplayer
void LD2450Component::set_multi_target(bool enable) {
  this->set_config_mode_(true);
  uint8_t cmd = enable ? CMD_MULTI_TARGET : CMD_SINGLE_TARGET;
  this->send_command_(cmd, nullptr, 0);
  this->set_config_mode_(false);
  // this->set_timeout(200, [this]() { this->read_all_info(); });
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

// Query Zone info from LD2450
void LD2450Component::query_zone_() { this->send_command_(CMD_QUERY_ZONE, nullptr, 0); }

#ifdef USE_SENSOR
void LD2450Component::set_move_x_sensor(int target, sensor::Sensor *s) { this->move_x_sensors_[target] = s; }
void LD2450Component::set_move_y_sensor(int target, sensor::Sensor *s) { this->move_y_sensors_[target] = s; }
void LD2450Component::set_move_speed_sensor(int target, sensor::Sensor *s) { this->move_speed_sensors_[target] = s; }
void LD2450Component::set_move_angle_sensor(int target, sensor::Sensor *s) { this->move_angle_sensors_[target] = s; }
void LD2450Component::set_move_distance_sensor(int target, sensor::Sensor *s) {
  this->move_distance_sensors_[target] = s;
}
void LD2450Component::set_move_resolution_sensor(int target, sensor::Sensor *s) {
  this->move_resolution_sensors_[target] = s;
}
#endif
#ifdef USE_TEXT_SENSOR
void LD2450Component::set_direction_text_sensor(int target, text_sensor::TextSensor *s) {
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
  zone_config_[zone].x1 = static_cast<int>(x1sens->state);
  zone_config_[zone].y1 = static_cast<int>(y1sens->state);
  zone_config_[zone].x2 = static_cast<int>(x2sens->state);
  zone_config_[zone].y2 = static_cast<int>(y2sens->state);
  this->send_set_zone_command_();
}

void LD2450Component::set_zone_x1_number(int zone, number::Number *n) { this->zone_x1_numbers_[zone] = n; }
void LD2450Component::set_zone_y1_number(int zone, number::Number *n) { this->zone_y1_numbers_[zone] = n; }
void LD2450Component::set_zone_x2_number(int zone, number::Number *n) { this->zone_x2_numbers_[zone] = n; }
void LD2450Component::set_zone_y2_number(int zone, number::Number *n) { this->zone_y2_numbers_[zone] = n; }
#endif

// Set Presence Timeout load and save from flash
#ifdef USE_NUMBER
void LD2450Component::set_presence_timeout() {
  if (this->presence_timeout_number_ != nullptr) {
    if (this->presence_timeout_number_->state == 0) {
      float timeout = this->restore_from_flash_();
      this->presence_timeout_number_->publish_state(timeout);
      this->timeout_ = this->convert_seconds_to_ms(timeout);
    }
    if (this->presence_timeout_number_->has_state()) {
      this->save_to_flash_(this->presence_timeout_number_->state);
      this->timeout_ = this->convert_seconds_to_ms(this->presence_timeout_number_->state);
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

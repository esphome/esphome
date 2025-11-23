#include "ld2410.h"

#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

#include "esphome/core/application.h"

namespace esphome::ld2410 {

static const char *const TAG = "ld2410";

enum BaudRate : uint8_t {
  BAUD_RATE_9600 = 1,
  BAUD_RATE_19200 = 2,
  BAUD_RATE_38400 = 3,
  BAUD_RATE_57600 = 4,
  BAUD_RATE_115200 = 5,
  BAUD_RATE_230400 = 6,
  BAUD_RATE_256000 = 7,
  BAUD_RATE_460800 = 8,
};

enum DistanceResolution : uint8_t {
  DISTANCE_RESOLUTION_0_2 = 0x01,
  DISTANCE_RESOLUTION_0_75 = 0x00,
};

enum LightFunction : uint8_t {
  LIGHT_FUNCTION_OFF = 0x00,
  LIGHT_FUNCTION_BELOW = 0x01,
  LIGHT_FUNCTION_ABOVE = 0x02,
};

enum OutPinLevel : uint8_t {
  OUT_PIN_LEVEL_LOW = 0x00,
  OUT_PIN_LEVEL_HIGH = 0x01,
};

enum PeriodicData : uint8_t {
  DATA_TYPES = 6,
  TARGET_STATES = 8,
  MOVING_TARGET_LOW = 9,
  MOVING_TARGET_HIGH = 10,
  MOVING_ENERGY = 11,
  STILL_TARGET_LOW = 12,
  STILL_TARGET_HIGH = 13,
  STILL_ENERGY = 14,
  DETECT_DISTANCE_LOW = 15,
  DETECT_DISTANCE_HIGH = 16,
  MOVING_SENSOR_START = 19,
  STILL_SENSOR_START = 28,
  LIGHT_SENSOR = 37,
  OUT_PIN_SENSOR = 38,
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

constexpr StringToUint8 DISTANCE_RESOLUTIONS_BY_STR[] = {
    {"0.2m", DISTANCE_RESOLUTION_0_2},
    {"0.75m", DISTANCE_RESOLUTION_0_75},
};

constexpr Uint8ToString DISTANCE_RESOLUTIONS_BY_UINT[] = {
    {DISTANCE_RESOLUTION_0_2, "0.2m"},
    {DISTANCE_RESOLUTION_0_75, "0.75m"},
};

constexpr StringToUint8 LIGHT_FUNCTIONS_BY_STR[] = {
    {"off", LIGHT_FUNCTION_OFF},
    {"below", LIGHT_FUNCTION_BELOW},
    {"above", LIGHT_FUNCTION_ABOVE},
};

constexpr Uint8ToString LIGHT_FUNCTIONS_BY_UINT[] = {
    {LIGHT_FUNCTION_OFF, "off"},
    {LIGHT_FUNCTION_BELOW, "below"},
    {LIGHT_FUNCTION_ABOVE, "above"},
};

constexpr StringToUint8 OUT_PIN_LEVELS_BY_STR[] = {
    {"low", OUT_PIN_LEVEL_LOW},
    {"high", OUT_PIN_LEVEL_HIGH},
};

constexpr Uint8ToString OUT_PIN_LEVELS_BY_UINT[] = {
    {OUT_PIN_LEVEL_LOW, "low"},
    {OUT_PIN_LEVEL_HIGH, "high"},
};

// Helper functions for lookups
template<size_t N> uint8_t find_uint8(const StringToUint8 (&arr)[N], const char *str) {
  for (const auto &entry : arr) {
    if (strcmp(str, entry.str) == 0)
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

// Commands
static constexpr uint8_t CMD_ENABLE_CONF = 0xFF;
static constexpr uint8_t CMD_DISABLE_CONF = 0xFE;
static constexpr uint8_t CMD_ENABLE_ENG = 0x62;
static constexpr uint8_t CMD_DISABLE_ENG = 0x63;
static constexpr uint8_t CMD_MAXDIST_DURATION = 0x60;
static constexpr uint8_t CMD_QUERY = 0x61;
static constexpr uint8_t CMD_GATE_SENS = 0x64;
static constexpr uint8_t CMD_QUERY_VERSION = 0xA0;
static constexpr uint8_t CMD_QUERY_DISTANCE_RESOLUTION = 0xAB;
static constexpr uint8_t CMD_SET_DISTANCE_RESOLUTION = 0xAA;
static constexpr uint8_t CMD_QUERY_LIGHT_CONTROL = 0xAE;
static constexpr uint8_t CMD_SET_LIGHT_CONTROL = 0xAD;
static constexpr uint8_t CMD_SET_BAUD_RATE = 0xA1;
static constexpr uint8_t CMD_BT_PASSWORD = 0xA9;
static constexpr uint8_t CMD_QUERY_MAC_ADDRESS = 0xA5;
static constexpr uint8_t CMD_RESET = 0xA2;
static constexpr uint8_t CMD_RESTART = 0xA3;
static constexpr uint8_t CMD_BLUETOOTH = 0xA4;
// Commands values
static constexpr uint8_t CMD_MAX_MOVE_VALUE = 0x00;
static constexpr uint8_t CMD_MAX_STILL_VALUE = 0x01;
static constexpr uint8_t CMD_DURATION_VALUE = 0x02;
// Bitmasks for target states
static constexpr uint8_t MOVE_BITMASK = 0x01;
static constexpr uint8_t STILL_BITMASK = 0x02;
// Header & Footer size
static constexpr uint8_t HEADER_FOOTER_SIZE = 4;
// Command Header & Footer
static constexpr uint8_t CMD_FRAME_HEADER[HEADER_FOOTER_SIZE] = {0xFD, 0xFC, 0xFB, 0xFA};
static constexpr uint8_t CMD_FRAME_FOOTER[HEADER_FOOTER_SIZE] = {0x04, 0x03, 0x02, 0x01};
// Data Header & Footer
static constexpr uint8_t DATA_FRAME_HEADER[HEADER_FOOTER_SIZE] = {0xF4, 0xF3, 0xF2, 0xF1};
static constexpr uint8_t DATA_FRAME_FOOTER[HEADER_FOOTER_SIZE] = {0xF8, 0xF7, 0xF6, 0xF5};
// MAC address the module uses when Bluetooth is disabled
static constexpr uint8_t NO_MAC[] = {0x08, 0x05, 0x04, 0x03, 0x02, 0x01};

static inline int two_byte_to_int(char firstbyte, char secondbyte) { return (int16_t) (secondbyte << 8) + firstbyte; }

static inline bool validate_header_footer(const uint8_t *header_footer, const uint8_t *buffer) {
  return std::memcmp(header_footer, buffer, HEADER_FOOTER_SIZE) == 0;
}

void LD2410Component::dump_config() {
  char mac_s[18];
  char version_s[20];
  const char *mac_str = ld24xx::format_mac_str(this->mac_address_, mac_s);
  ld24xx::format_version_str(this->version_, version_s);
  ESP_LOGCONFIG(TAG,
                "LD2410:\n"
                "  Firmware version: %s\n"
                "  MAC address: %s",
                version_s, mac_str);
#ifdef USE_BINARY_SENSOR
  ESP_LOGCONFIG(TAG, "Binary Sensors:");
  LOG_BINARY_SENSOR("  ", "Target", this->target_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "MovingTarget", this->moving_target_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "StillTarget", this->still_target_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "OutPinPresenceStatus", this->out_pin_presence_status_binary_sensor_);
#endif
#ifdef USE_SENSOR
  ESP_LOGCONFIG(TAG, "Sensors:");
  LOG_SENSOR_WITH_DEDUP_SAFE("  ", "Light", this->light_sensor_);
  LOG_SENSOR_WITH_DEDUP_SAFE("  ", "DetectionDistance", this->detection_distance_sensor_);
  LOG_SENSOR_WITH_DEDUP_SAFE("  ", "MovingTargetDistance", this->moving_target_distance_sensor_);
  LOG_SENSOR_WITH_DEDUP_SAFE("  ", "MovingTargetEnergy", this->moving_target_energy_sensor_);
  LOG_SENSOR_WITH_DEDUP_SAFE("  ", "StillTargetDistance", this->still_target_distance_sensor_);
  LOG_SENSOR_WITH_DEDUP_SAFE("  ", "StillTargetEnergy", this->still_target_energy_sensor_);
  for (auto &s : this->gate_move_sensors_) {
    LOG_SENSOR_WITH_DEDUP_SAFE("  ", "GateMove", s);
  }
  for (auto &s : this->gate_still_sensors_) {
    LOG_SENSOR_WITH_DEDUP_SAFE("  ", "GateStill", s);
  }
#endif
#ifdef USE_TEXT_SENSOR
  ESP_LOGCONFIG(TAG, "Text Sensors:");
  LOG_TEXT_SENSOR("  ", "Mac", this->mac_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Version", this->version_text_sensor_);
#endif
#ifdef USE_NUMBER
  ESP_LOGCONFIG(TAG, "Numbers:");
  LOG_NUMBER("  ", "LightThreshold", this->light_threshold_number_);
  LOG_NUMBER("  ", "MaxMoveDistanceGate", this->max_move_distance_gate_number_);
  LOG_NUMBER("  ", "MaxStillDistanceGate", this->max_still_distance_gate_number_);
  LOG_NUMBER("  ", "Timeout", this->timeout_number_);
  for (number::Number *n : this->gate_move_threshold_numbers_) {
    LOG_NUMBER("  ", "MoveThreshold", n);
  }
  for (number::Number *n : this->gate_still_threshold_numbers_) {
    LOG_NUMBER("  ", "StillThreshold", n);
  }
#endif
#ifdef USE_SELECT
  ESP_LOGCONFIG(TAG, "Selects:");
  LOG_SELECT("  ", "BaudRate", this->baud_rate_select_);
  LOG_SELECT("  ", "DistanceResolution", this->distance_resolution_select_);
  LOG_SELECT("  ", "LightFunction", this->light_function_select_);
  LOG_SELECT("  ", "OutPinLevel", this->out_pin_level_select_);
#endif
#ifdef USE_SWITCH
  ESP_LOGCONFIG(TAG, "Switches:");
  LOG_SWITCH("  ", "Bluetooth", this->bluetooth_switch_);
  LOG_SWITCH("  ", "EngineeringMode", this->engineering_mode_switch_);
#endif
#ifdef USE_BUTTON
  ESP_LOGCONFIG(TAG, "Buttons:");
  LOG_BUTTON("  ", "FactoryReset", this->factory_reset_button_);
  LOG_BUTTON("  ", "Query", this->query_button_);
  LOG_BUTTON("  ", "Restart", this->restart_button_);
#endif
}

void LD2410Component::setup() { this->read_all_info(); }

void LD2410Component::read_all_info() {
  this->set_config_mode_(true);
  this->get_version_();
  this->get_mac_();
  this->get_distance_resolution_();
  this->query_light_control_();
  this->query_parameters_();
  this->set_config_mode_(false);
#ifdef USE_SELECT
  const auto baud_rate = std::to_string(this->parent_->get_baud_rate());
  if (this->baud_rate_select_ != nullptr) {
    this->baud_rate_select_->publish_state(baud_rate);
  }
#endif
}

void LD2410Component::restart_and_read_all_info() {
  this->set_config_mode_(true);
  this->restart_();
  this->set_timeout(1000, [this]() { this->read_all_info(); });
}

void LD2410Component::loop() {
  while (this->available()) {
    this->readline_(this->read());
  }
}

void LD2410Component::send_command_(uint8_t command, const uint8_t *command_value, uint8_t command_value_len) {
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

void LD2410Component::handle_periodic_data_() {
  // 4 frame header bytes + 2 length bytes + 1 data end byte + 1 crc byte + 4 frame footer bytes
  // data header=0xAA, data footer=0x55, crc=0x00
  if (this->buffer_pos_ < 12 || !ld2410::validate_header_footer(DATA_FRAME_HEADER, this->buffer_data_) ||
      this->buffer_data_[7] != HEADER || this->buffer_data_[this->buffer_pos_ - 6] != FOOTER ||
      this->buffer_data_[this->buffer_pos_ - 5] != CHECK) {
    return;
  }
  /*
    Data Type: 7th
    0x01: Engineering mode
    0x02: Normal mode
  */
  bool engineering_mode = this->buffer_data_[DATA_TYPES] == 0x01;
#ifdef USE_SWITCH
  if (this->engineering_mode_switch_ != nullptr) {
    this->engineering_mode_switch_->publish_state(engineering_mode);
  }
#endif
#ifdef USE_BINARY_SENSOR
  /*
    Target states: 9th
    0x00 = No target
    0x01 = Moving targets
    0x02 = Still targets
    0x03 = Moving+Still targets
  */
  char target_state = this->buffer_data_[TARGET_STATES];
  if (this->target_binary_sensor_ != nullptr) {
    this->target_binary_sensor_->publish_state(target_state != 0x00);
  }
  if (this->moving_target_binary_sensor_ != nullptr) {
    this->moving_target_binary_sensor_->publish_state(target_state & MOVE_BITMASK);
  }
  if (this->still_target_binary_sensor_ != nullptr) {
    this->still_target_binary_sensor_->publish_state(target_state & STILL_BITMASK);
  }
#endif
  /*
    Moving target distance: 10~11th bytes
    Moving target energy: 12th byte
    Still target distance: 13~14th bytes
    Still target energy: 15th byte
    Detect distance: 16~17th bytes
  */
#ifdef USE_SENSOR
  SAFE_PUBLISH_SENSOR(
      this->moving_target_distance_sensor_,
      ld2410::two_byte_to_int(this->buffer_data_[MOVING_TARGET_LOW], this->buffer_data_[MOVING_TARGET_HIGH]))
  SAFE_PUBLISH_SENSOR(this->moving_target_energy_sensor_, this->buffer_data_[MOVING_ENERGY])
  SAFE_PUBLISH_SENSOR(
      this->still_target_distance_sensor_,
      ld2410::two_byte_to_int(this->buffer_data_[STILL_TARGET_LOW], this->buffer_data_[STILL_TARGET_HIGH]));
  SAFE_PUBLISH_SENSOR(this->still_target_energy_sensor_, this->buffer_data_[STILL_ENERGY]);
  SAFE_PUBLISH_SENSOR(
      this->detection_distance_sensor_,
      ld2410::two_byte_to_int(this->buffer_data_[DETECT_DISTANCE_LOW], this->buffer_data_[DETECT_DISTANCE_HIGH]));

  if (engineering_mode) {
    /*
      Moving distance range: 18th byte
      Still distance range: 19th byte
      Moving energy: 20~28th bytes
    */
    for (uint8_t i = 0; i < TOTAL_GATES; i++) {
      SAFE_PUBLISH_SENSOR(this->gate_move_sensors_[i], this->buffer_data_[MOVING_SENSOR_START + i])
    }
    /*
      Still energy: 29~37th bytes
    */
    for (uint8_t i = 0; i < TOTAL_GATES; i++) {
      SAFE_PUBLISH_SENSOR(this->gate_still_sensors_[i], this->buffer_data_[STILL_SENSOR_START + i])
    }
    /*
      Light sensor: 38th bytes
    */
    SAFE_PUBLISH_SENSOR(this->light_sensor_, this->buffer_data_[LIGHT_SENSOR])
  } else {
    for (auto &gate_move_sensor : this->gate_move_sensors_) {
      SAFE_PUBLISH_SENSOR_UNKNOWN(gate_move_sensor)
    }
    for (auto &gate_still_sensor : this->gate_still_sensors_) {
      SAFE_PUBLISH_SENSOR_UNKNOWN(gate_still_sensor)
    }
    SAFE_PUBLISH_SENSOR_UNKNOWN(this->light_sensor_)
  }
#endif
#ifdef USE_BINARY_SENSOR
  if (this->out_pin_presence_status_binary_sensor_ != nullptr) {
    this->out_pin_presence_status_binary_sensor_->publish_state(
        engineering_mode ? this->buffer_data_[OUT_PIN_SENSOR] == 0x01 : false);
  }
#endif
}

#ifdef USE_NUMBER
std::function<void(void)> set_number_value(number::Number *n, float value) {
  if (n != nullptr && (!n->has_state() || n->state != value)) {
    n->state = value;
    return [n, value]() { n->publish_state(value); };
  }
  return []() {};
}
#endif

bool LD2410Component::handle_ack_data_() {
  ESP_LOGV(TAG, "Handling ACK DATA for COMMAND %02X", this->buffer_data_[COMMAND]);
  if (this->buffer_pos_ < 10) {
    ESP_LOGE(TAG, "Invalid length");
    return true;
  }
  if (!ld2410::validate_header_footer(CMD_FRAME_HEADER, this->buffer_data_)) {
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

    case CMD_QUERY_DISTANCE_RESOLUTION: {
      const auto *distance_resolution = find_str(DISTANCE_RESOLUTIONS_BY_UINT, this->buffer_data_[10]);
      ESP_LOGV(TAG, "Distance resolution: %s", distance_resolution);
#ifdef USE_SELECT
      if (this->distance_resolution_select_ != nullptr) {
        this->distance_resolution_select_->publish_state(distance_resolution);
      }
#endif
      break;
    }

    case CMD_QUERY_LIGHT_CONTROL: {
      this->light_function_ = this->buffer_data_[10];
      this->light_threshold_ = this->buffer_data_[11];
      this->out_pin_level_ = this->buffer_data_[12];
      const auto *light_function_str = find_str(LIGHT_FUNCTIONS_BY_UINT, this->light_function_);
      const auto *out_pin_level_str = find_str(OUT_PIN_LEVELS_BY_UINT, this->out_pin_level_);
      ESP_LOGV(TAG,
               "Light function: %s\n"
               "Light threshold: %u\n"
               "Out pin level: %s",
               light_function_str, this->light_threshold_, out_pin_level_str);
#ifdef USE_SELECT
      if (this->light_function_select_ != nullptr) {
        this->light_function_select_->publish_state(light_function_str);
      }
      if (this->out_pin_level_select_ != nullptr) {
        this->out_pin_level_select_->publish_state(out_pin_level_str);
      }
#endif
#ifdef USE_NUMBER
      if (this->light_threshold_number_ != nullptr) {
        this->light_threshold_number_->publish_state(static_cast<float>(this->light_threshold_));
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

    case CMD_GATE_SENS:
      ESP_LOGV(TAG, "Sensitivity");
      break;

    case CMD_BLUETOOTH:
      ESP_LOGV(TAG, "Bluetooth");
      break;

    case CMD_SET_DISTANCE_RESOLUTION:
      ESP_LOGV(TAG, "Set distance resolution");
      break;

    case CMD_SET_LIGHT_CONTROL:
      ESP_LOGV(TAG, "Set light control");
      break;

    case CMD_BT_PASSWORD:
      ESP_LOGV(TAG, "Set bluetooth password");
      break;

    case CMD_QUERY: {  // Query parameters response
      if (this->buffer_data_[10] != HEADER)
        return true;  // value head=0xAA
#ifdef USE_NUMBER
      /*
        Moving distance range: 13th byte
        Still distance range: 14th byte
      */
      std::vector<std::function<void(void)>> updates;
      updates.push_back(set_number_value(this->max_move_distance_gate_number_, this->buffer_data_[12]));
      updates.push_back(set_number_value(this->max_still_distance_gate_number_, this->buffer_data_[13]));
      /*
        Moving Sensitivities: 15~23th bytes
      */
      for (std::vector<number::Number *>::size_type i = 0; i != this->gate_move_threshold_numbers_.size(); i++) {
        updates.push_back(set_number_value(this->gate_move_threshold_numbers_[i], this->buffer_data_[14 + i]));
      }
      /*
        Still Sensitivities: 24~32th bytes
      */
      for (std::vector<number::Number *>::size_type i = 0; i != this->gate_still_threshold_numbers_.size(); i++) {
        updates.push_back(set_number_value(this->gate_still_threshold_numbers_[i], this->buffer_data_[23 + i]));
      }
      /*
        None Duration: 33~34th bytes
      */
      updates.push_back(set_number_value(this->timeout_number_,
                                         ld2410::two_byte_to_int(this->buffer_data_[32], this->buffer_data_[33])));
      for (auto &update : updates) {
        update();
      }
#endif
      break;
    }
    default:
      break;
  }

  return true;
}

void LD2410Component::readline_(int readch) {
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
  if (ld2410::validate_header_footer(DATA_FRAME_FOOTER, &this->buffer_data_[this->buffer_pos_ - 4])) {
    ESP_LOGV(TAG, "Handling Periodic Data: %s", format_hex_pretty(this->buffer_data_, this->buffer_pos_).c_str());
    this->handle_periodic_data_();
    this->buffer_pos_ = 0;  // Reset position index for next message
  } else if (ld2410::validate_header_footer(CMD_FRAME_FOOTER, &this->buffer_data_[this->buffer_pos_ - 4])) {
    ESP_LOGV(TAG, "Handling Ack Data: %s", format_hex_pretty(this->buffer_data_, this->buffer_pos_).c_str());
    if (this->handle_ack_data_()) {
      this->buffer_pos_ = 0;  // Reset position index for next message
    } else {
      ESP_LOGV(TAG, "Ack Data incomplete");
    }
  }
}

void LD2410Component::set_config_mode_(bool enable) {
  const uint8_t cmd = enable ? CMD_ENABLE_CONF : CMD_DISABLE_CONF;
  const uint8_t cmd_value[2] = {0x01, 0x00};
  this->send_command_(cmd, enable ? cmd_value : nullptr, sizeof(cmd_value));
}

void LD2410Component::set_bluetooth(bool enable) {
  this->set_config_mode_(true);
  const uint8_t cmd_value[2] = {enable ? (uint8_t) 0x01 : (uint8_t) 0x00, 0x00};
  this->send_command_(CMD_BLUETOOTH, cmd_value, sizeof(cmd_value));
  this->set_timeout(200, [this]() { this->restart_and_read_all_info(); });
}

void LD2410Component::set_distance_resolution(const char *state) {
  this->set_config_mode_(true);
  const uint8_t cmd_value[2] = {find_uint8(DISTANCE_RESOLUTIONS_BY_STR, state), 0x00};
  this->send_command_(CMD_SET_DISTANCE_RESOLUTION, cmd_value, sizeof(cmd_value));
  this->set_timeout(200, [this]() { this->restart_and_read_all_info(); });
}

void LD2410Component::set_baud_rate(const char *state) {
  this->set_config_mode_(true);
  const uint8_t cmd_value[2] = {find_uint8(BAUD_RATES_BY_STR, state), 0x00};
  this->send_command_(CMD_SET_BAUD_RATE, cmd_value, sizeof(cmd_value));
  this->set_timeout(200, [this]() { this->restart_(); });
}

void LD2410Component::set_bluetooth_password(const std::string &password) {
  if (password.length() != 6) {
    ESP_LOGE(TAG, "Password must be exactly 6 chars");
    return;
  }
  this->set_config_mode_(true);
  uint8_t cmd_value[6];
  std::copy(password.begin(), password.end(), std::begin(cmd_value));
  this->send_command_(CMD_BT_PASSWORD, cmd_value, sizeof(cmd_value));
  this->set_config_mode_(false);
}

void LD2410Component::set_engineering_mode(bool enable) {
  const uint8_t cmd = enable ? CMD_ENABLE_ENG : CMD_DISABLE_ENG;
  this->set_config_mode_(true);
  this->send_command_(cmd, nullptr, 0);
  this->set_config_mode_(false);
}

void LD2410Component::factory_reset() {
  this->set_config_mode_(true);
  this->send_command_(CMD_RESET, nullptr, 0);
  this->set_timeout(200, [this]() { this->restart_and_read_all_info(); });
}

void LD2410Component::restart_() { this->send_command_(CMD_RESTART, nullptr, 0); }

void LD2410Component::query_parameters_() { this->send_command_(CMD_QUERY, nullptr, 0); }

void LD2410Component::get_version_() { this->send_command_(CMD_QUERY_VERSION, nullptr, 0); }

void LD2410Component::get_mac_() {
  const uint8_t cmd_value[2] = {0x01, 0x00};
  this->send_command_(CMD_QUERY_MAC_ADDRESS, cmd_value, sizeof(cmd_value));
}

void LD2410Component::get_distance_resolution_() { this->send_command_(CMD_QUERY_DISTANCE_RESOLUTION, nullptr, 0); }

void LD2410Component::query_light_control_() { this->send_command_(CMD_QUERY_LIGHT_CONTROL, nullptr, 0); }

#ifdef USE_NUMBER
void LD2410Component::set_max_distances_timeout() {
  if (!this->max_move_distance_gate_number_->has_state() || !this->max_still_distance_gate_number_->has_state() ||
      !this->timeout_number_->has_state()) {
    return;
  }
  int max_moving_distance_gate_range = static_cast<int>(this->max_move_distance_gate_number_->state);
  int max_still_distance_gate_range = static_cast<int>(this->max_still_distance_gate_number_->state);
  int timeout = static_cast<int>(this->timeout_number_->state);
  uint8_t value[18] = {0x00,
                       0x00,
                       lowbyte(max_moving_distance_gate_range),
                       highbyte(max_moving_distance_gate_range),
                       0x00,
                       0x00,
                       0x01,
                       0x00,
                       lowbyte(max_still_distance_gate_range),
                       highbyte(max_still_distance_gate_range),
                       0x00,
                       0x00,
                       0x02,
                       0x00,
                       lowbyte(timeout),
                       highbyte(timeout),
                       0x00,
                       0x00};
  this->set_config_mode_(true);
  this->send_command_(CMD_MAXDIST_DURATION, value, sizeof(value));
  this->query_parameters_();
  this->set_timeout(200, [this]() { this->restart_and_read_all_info(); });
  this->set_config_mode_(false);
}

void LD2410Component::set_gate_threshold(uint8_t gate) {
  number::Number *motionsens = this->gate_move_threshold_numbers_[gate];
  number::Number *stillsens = this->gate_still_threshold_numbers_[gate];

  if (!motionsens->has_state() || !stillsens->has_state()) {
    return;
  }
  int motion = static_cast<int>(motionsens->state);
  int still = static_cast<int>(stillsens->state);

  this->set_config_mode_(true);
  // reference
  // https://drive.google.com/drive/folders/1p4dhbEJA3YubyIjIIC7wwVsSo8x29Fq-?spm=a2g0o.detail.1000023.17.93465697yFwVxH
  //   Send data: configure the motion sensitivity of distance gate 3 to 40, and the static sensitivity of 40
  // 00 00 (gate)
  // 03 00 00 00 (gate number)
  // 01 00 (motion sensitivity)
  // 28 00 00 00 (value)
  // 02 00 (still sensitivtiy)
  // 28 00 00 00 (value)
  uint8_t value[18] = {0x00, 0x00, lowbyte(gate),   highbyte(gate),   0x00, 0x00,
                       0x01, 0x00, lowbyte(motion), highbyte(motion), 0x00, 0x00,
                       0x02, 0x00, lowbyte(still),  highbyte(still),  0x00, 0x00};
  this->send_command_(CMD_GATE_SENS, value, sizeof(value));
  this->query_parameters_();
  this->set_config_mode_(false);
}

void LD2410Component::set_gate_still_threshold_number(uint8_t gate, number::Number *n) {
  this->gate_still_threshold_numbers_[gate] = n;
}

void LD2410Component::set_gate_move_threshold_number(uint8_t gate, number::Number *n) {
  this->gate_move_threshold_numbers_[gate] = n;
}
#endif

void LD2410Component::set_light_out_control() {
#ifdef USE_NUMBER
  if (this->light_threshold_number_ != nullptr && this->light_threshold_number_->has_state()) {
    this->light_threshold_ = static_cast<uint8_t>(this->light_threshold_number_->state);
  }
#endif
#ifdef USE_SELECT
  if (this->light_function_select_ != nullptr && this->light_function_select_->has_state()) {
    this->light_function_ = find_uint8(LIGHT_FUNCTIONS_BY_STR, this->light_function_select_->current_option());
  }
  if (this->out_pin_level_select_ != nullptr && this->out_pin_level_select_->has_state()) {
    this->out_pin_level_ = find_uint8(OUT_PIN_LEVELS_BY_STR, this->out_pin_level_select_->current_option());
  }
#endif
  this->set_config_mode_(true);
  uint8_t value[4] = {this->light_function_, this->light_threshold_, this->out_pin_level_, 0x00};
  this->send_command_(CMD_SET_LIGHT_CONTROL, value, sizeof(value));
  this->query_light_control_();
  this->set_timeout(200, [this]() { this->restart_and_read_all_info(); });
  this->set_config_mode_(false);
}

#ifdef USE_SENSOR
// These could leak memory, but they are only set once prior to 'setup()' and should never be used again.
void LD2410Component::set_gate_move_sensor(uint8_t gate, sensor::Sensor *s) {
  this->gate_move_sensors_[gate] = new SensorWithDedup<uint8_t>(s);
}

void LD2410Component::set_gate_still_sensor(uint8_t gate, sensor::Sensor *s) {
  this->gate_still_sensors_[gate] = new SensorWithDedup<uint8_t>(s);
}
#endif

}  // namespace esphome::ld2410

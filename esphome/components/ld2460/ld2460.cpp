#include "ld2460.h"

#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <cinttypes>
#include <cmath>
#include <cstring>
#include <numbers>

namespace esphome::ld2460 {

static const char *const TAG = "ld2460";

enum BaudRate : uint8_t {
  BAUD_RATE_9600 = 0,
  BAUD_RATE_19200 = 1,
  BAUD_RATE_38400 = 2,
  BAUD_RATE_57600 = 3,
  BAUD_RATE_115200 = 4,
  BAUD_RATE_230400 = 5,
  BAUD_RATE_256000 = 6,
  BAUD_RATE_460800 = 7,
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

constexpr uint32_t BAUD_RATES[] = {9600, 19200, 38400, 57600, 115200, 230400, 256000, 460800};

constexpr Uint8ToString INSTALLATION_MODE_BY_UINT[] = {
    {MODE_SIDE, "Side"},
    {MODE_TOP, "Top"},
};

constexpr StringToUint8 INSTALLATION_MODE_BY_STR[] = {
    {"Side", MODE_SIDE},
    {"Top", MODE_TOP},
};

constexpr Uint8ToString SENSITIVITY_BY_UINT[] = {
    {SENSITIVITY_HIGH, "High"},
    {SENSITIVITY_MEDIUM, "Medium"},
    {SENSITIVITY_LOW, "Low"},
};

constexpr StringToUint8 SENSITIVITY_BY_STR[] = {
    {"High", SENSITIVITY_HIGH},
    {"Medium", SENSITIVITY_MEDIUM},
    {"Low", SENSITIVITY_LOW},
};

// Helper functions for lookups
template<size_t N> uint8_t find_uint8(const StringToUint8 (&arr)[N], const std::string &str) {
  for (const auto &entry : arr) {
    if (str == entry.str)
      return entry.value;
  }
  return 0xFF;
}

template<size_t N> const char *find_str(const Uint8ToString (&arr)[N], uint8_t value) {
  for (const auto &entry : arr) {
    if (value == entry.value)
      return entry.str;
  }
  return "";
}

// LD2460 Frame Headers & Footers
static constexpr uint8_t HEADER_FOOTER_SIZE = 4;
static constexpr uint8_t CMD_FRAME_HEADER[HEADER_FOOTER_SIZE] = {0xFD, 0xFC, 0xFB, 0xFA};
static constexpr uint8_t CMD_FRAME_FOOTER[HEADER_FOOTER_SIZE] = {0x04, 0x03, 0x02, 0x01};
static constexpr uint8_t DATA_FRAME_HEADER[HEADER_FOOTER_SIZE] = {0xF4, 0xF3, 0xF2, 0xF1};
static constexpr uint8_t DATA_FRAME_FOOTER[HEADER_FOOTER_SIZE] = {0xF8, 0xF7, 0xF6, 0xF5};

// LD2460 Command Codes
static constexpr uint8_t CMD_REPORT_DATA = 0x04;
static constexpr uint8_t CMD_SET_REPORTING = 0x06;
static constexpr uint8_t CMD_SET_INSTALLATION_PARAMS = 0x07;
static constexpr uint8_t CMD_QUERY_INSTALLATION_PARAMS = 0x08;
static constexpr uint8_t CMD_SET_INSTALLATION_MODE = 0x09;
static constexpr uint8_t CMD_QUERY_INSTALLATION_MODE = 0x0A;
static constexpr uint8_t CMD_QUERY_VERSION = 0x0B;
static constexpr uint8_t CMD_RESTART = 0x0D;
static constexpr uint8_t CMD_SET_BAUD_RATE = 0x0E;
static constexpr uint8_t CMD_FACTORY_RESET = 0x10;
static constexpr uint8_t CMD_SET_DETECTION_RANGE = 0x11;
static constexpr uint8_t CMD_QUERY_DETECTION_RANGE = 0x12;
static constexpr uint8_t CMD_SET_SENSITIVITY = 0x13;
static constexpr uint8_t CMD_QUERY_SENSITIVITY = 0x14;

static inline bool validate_header_footer(const uint8_t *expected, const uint8_t *buffer) {
  return std::memcmp(expected, buffer, HEADER_FOOTER_SIZE) == 0;
}

void LD2460Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up LD2460...");
  this->read_all_info();
}

void LD2460Component::dump_config() {
  ESP_LOGCONFIG(TAG, "LD2460:");
#ifdef USE_BINARY_SENSOR
  ESP_LOGCONFIG(TAG, "  Binary Sensors:");
  LOG_BINARY_SENSOR("    ", "Target", this->target_binary_sensor_);
#endif
#ifdef USE_SENSOR
  ESP_LOGCONFIG(TAG, "  Sensors:");
  LOG_SENSOR_WITH_DEDUP_SAFE("    ", "TargetCount", this->target_count_sensor_);
  for (size_t i = 0; i < MAX_TARGETS; i++) {
    LOG_SENSOR_WITH_DEDUP_SAFE("    ", "TargetX", this->target_x_sensors_[i]);
    LOG_SENSOR_WITH_DEDUP_SAFE("    ", "TargetY", this->target_y_sensors_[i]);
    LOG_SENSOR_WITH_DEDUP_SAFE("    ", "TargetDistance", this->target_distance_sensors_[i]);
    LOG_SENSOR_WITH_DEDUP_SAFE("    ", "TargetAngle", this->target_angle_sensors_[i]);
  }
#endif
#ifdef USE_TEXT_SENSOR
  ESP_LOGCONFIG(TAG, "  Text Sensors:");
  LOG_TEXT_SENSOR("    ", "Version", this->version_text_sensor_);
#endif
#ifdef USE_NUMBER
  ESP_LOGCONFIG(TAG, "  Numbers:");
  LOG_NUMBER("    ", "InstallationHeight", this->installation_height_number_);
  LOG_NUMBER("    ", "InstallationAngle", this->installation_angle_number_);
  LOG_NUMBER("    ", "DetectionDistance", this->detection_distance_number_);
  LOG_NUMBER("    ", "DetectionAngleMin", this->detection_angle_min_number_);
  LOG_NUMBER("    ", "DetectionAngleMax", this->detection_angle_max_number_);
#endif
#ifdef USE_SELECT
  ESP_LOGCONFIG(TAG, "  Selects:");
  LOG_SELECT("    ", "BaudRate", this->baud_rate_select_);
  LOG_SELECT("    ", "InstallationMode", this->installation_mode_select_);
  LOG_SELECT("    ", "Sensitivity", this->sensitivity_select_);
#endif
#ifdef USE_SWITCH
  ESP_LOGCONFIG(TAG, "  Switches:");
  LOG_SWITCH("    ", "Reporting", this->reporting_switch_);
#endif
#ifdef USE_BUTTON
  ESP_LOGCONFIG(TAG, "  Buttons:");
  LOG_BUTTON("    ", "FactoryReset", this->factory_reset_button_);
  LOG_BUTTON("    ", "Restart", this->restart_button_);
#endif
}

void LD2460Component::loop() {
  size_t avail = this->available();
  uint8_t buf[MAX_LINE_LENGTH];
  while (avail > 0) {
    size_t to_read = std::min(avail, sizeof(buf));
    if (!this->read_array(buf, to_read)) {
      break;
    }
    avail -= to_read;

    for (size_t i = 0; i < to_read; i++) {
      this->readline_(buf[i]);
    }
  }
}

void LD2460Component::read_all_info() {
  this->query_version_();
  this->query_installation_mode_();
  this->query_installation_params_();
  this->query_detection_range_();
  this->query_sensitivity_();
#ifdef USE_SELECT
  if (this->baud_rate_select_ != nullptr) {
    if (auto index = ld24xx::find_index(BAUD_RATES, this->parent_->get_baud_rate())) {
      this->baud_rate_select_->publish_state(*index);
    }
  }
#endif
}

void LD2460Component::restart_and_read_all_info() {
  this->restart();
  this->set_timeout(1500, [this]() { this->read_all_info(); });
}

void LD2460Component::send_command_(uint8_t command, const uint8_t *data, uint8_t data_len) {
  ESP_LOGV(TAG, "Sending COMMAND %02X", command);
  this->write_array(CMD_FRAME_HEADER, HEADER_FOOTER_SIZE);
  this->write_byte(command);
  uint16_t total_len = 11 + data_len;
  uint8_t len_bytes[2] = {static_cast<uint8_t>(total_len & 0xFF), static_cast<uint8_t>((total_len >> 8) & 0xFF)};
  this->write_array(len_bytes, sizeof(len_bytes));
  if (data != nullptr && data_len > 0) {
    this->write_array(data, data_len);
  }
  this->write_array(CMD_FRAME_FOOTER, HEADER_FOOTER_SIZE);
  delay(50);  // NOLINT
}

void LD2460Component::handle_periodic_data_() {
  if (this->buffer_pos_ < 11) {
    ESP_LOGE(TAG, "Invalid periodic data length: %u", this->buffer_pos_);
    return;
  }
  if (!validate_header_footer(DATA_FRAME_HEADER, this->buffer_data_) ||
      !validate_header_footer(DATA_FRAME_FOOTER, &this->buffer_data_[this->buffer_pos_ - 4])) {
    ESP_LOGE(TAG, "Invalid periodic header or footer");
    return;
  }
  if (this->buffer_data_[4] != CMD_REPORT_DATA) {
    ESP_LOGE(TAG, "Invalid function code for periodic data: %02X", this->buffer_data_[4]);
    return;
  }

  uint16_t packet_len = this->buffer_data_[5] | (this->buffer_data_[6] << 8);
  if (packet_len != this->buffer_pos_) {
    ESP_LOGW(TAG, "Periodic packet length mismatch: %u != %u", packet_len, this->buffer_pos_);
    return;
  }

  uint8_t num_targets = (packet_len >= 11) ? ((packet_len - 11) / 4) : 0;
  if (num_targets > MAX_TARGETS) {
    num_targets = MAX_TARGETS;
  }

  for (uint8_t i = 0; i < MAX_TARGETS; i++) {
    if (i < num_targets) {
      uint8_t offset = 7 + i * 4;
      int16_t raw_x = static_cast<int16_t>(this->buffer_data_[offset] | (this->buffer_data_[offset + 1] << 8));
      int16_t raw_y = static_cast<int16_t>(this->buffer_data_[offset + 2] | (this->buffer_data_[offset + 3] << 8));

      // Accuracy 0.1m -> meters
      float x = raw_x * 0.1f;
      float y = raw_y * 0.1f;
      float distance = std::sqrt(x * x + y * y);

      float angle;
      if (this->installation_mode_ == MODE_TOP) {
        angle = std::atan2(y, x) * (180.0f / std::numbers::pi_v<float>);
        if (angle < 0.0f) {
          angle += 360.0f;
        }
      } else {
        angle = std::atan2(x, y) * (180.0f / std::numbers::pi_v<float>);
      }

      this->target_info_[i].x = x;
      this->target_info_[i].y = y;
      this->target_info_[i].distance = distance;
      this->target_info_[i].angle = angle;

#ifdef USE_SENSOR
      SAFE_PUBLISH_SENSOR(this->target_x_sensors_[i], x);
      SAFE_PUBLISH_SENSOR(this->target_y_sensors_[i], y);
      SAFE_PUBLISH_SENSOR(this->target_distance_sensors_[i], distance);
      SAFE_PUBLISH_SENSOR(this->target_angle_sensors_[i], angle);
#endif
    } else {
      this->target_info_[i].x = 0.0f;
      this->target_info_[i].y = 0.0f;
      this->target_info_[i].distance = 0.0f;
      this->target_info_[i].angle = 0.0f;

#ifdef USE_SENSOR
      SAFE_PUBLISH_SENSOR_UNKNOWN(this->target_x_sensors_[i]);
      SAFE_PUBLISH_SENSOR_UNKNOWN(this->target_y_sensors_[i]);
      SAFE_PUBLISH_SENSOR_UNKNOWN(this->target_distance_sensors_[i]);
      SAFE_PUBLISH_SENSOR_UNKNOWN(this->target_angle_sensors_[i]);
#endif
    }
  }

#ifdef USE_SENSOR
  SAFE_PUBLISH_SENSOR(this->target_count_sensor_, num_targets);
#endif

#ifdef USE_BINARY_SENSOR
  if (this->target_binary_sensor_ != nullptr) {
    this->target_binary_sensor_->publish_state(num_targets > 0);
  }
#endif

  this->data_callback_.call();
}

bool LD2460Component::handle_ack_data_() {
  if (this->buffer_pos_ < 11) {
    ESP_LOGE(TAG, "Invalid ACK length: %u", this->buffer_pos_);
    return true;
  }
  if (!validate_header_footer(CMD_FRAME_HEADER, this->buffer_data_)) {
    char hex_buf[format_hex_pretty_size(HEADER_FOOTER_SIZE)];
    ESP_LOGW(TAG, "Invalid ACK header: %s", format_hex_pretty_to(hex_buf, this->buffer_data_, HEADER_FOOTER_SIZE));
    return true;
  }

  uint8_t cmd = this->buffer_data_[4];
  uint16_t packet_len = this->buffer_data_[5] | (this->buffer_data_[6] << 8);
  if (packet_len != this->buffer_pos_) {
    ESP_LOGW(TAG, "ACK packet length mismatch: %u != %u", packet_len, this->buffer_pos_);
    return true;
  }

  ESP_LOGV(TAG, "Handling ACK DATA for COMMAND %02X", cmd);

  switch (cmd) {
    case CMD_SET_REPORTING: {
      uint8_t val = this->buffer_data_[7];
      bool success = (val >> 4) == 1;
      bool enabled = (val & 0x0F) == 1;
      ESP_LOGV(TAG, "Set reporting ACK: success=%s, enabled=%s", YESNO(success), YESNO(enabled));
#ifdef USE_SWITCH
      if (this->reporting_switch_ != nullptr && success) {
        this->reporting_switch_->publish_state(enabled);
      }
#endif
      break;
    }

    case CMD_SET_INSTALLATION_PARAMS: {
      bool success = this->buffer_data_[7] == 0x01;
      ESP_LOGV(TAG, "Set installation params ACK: %s", success ? "Success" : "Failed");
      break;
    }

    case CMD_QUERY_INSTALLATION_PARAMS: {
      uint16_t height_cm = this->buffer_data_[7] | (this->buffer_data_[8] << 8);
      uint16_t angle_x100 = this->buffer_data_[9] | (this->buffer_data_[10] << 8);
      this->installation_height_ = height_cm / 100.0f;
      this->installation_angle_ = angle_x100 / 100.0f;
      ESP_LOGV(TAG, "Installation height: %.2fm, angle: %.2f deg", this->installation_height_,
               this->installation_angle_);
#ifdef USE_NUMBER
      if (this->installation_height_number_ != nullptr) {
        this->installation_height_number_->publish_state(this->installation_height_);
      }
      if (this->installation_angle_number_ != nullptr) {
        this->installation_angle_number_->publish_state(this->installation_angle_);
      }
#endif
      break;
    }

    case CMD_SET_INSTALLATION_MODE: {
      uint8_t val = this->buffer_data_[7];
      bool success = (val >> 4) == 1;
      uint8_t mode = val & 0x0F;
      ESP_LOGV(TAG, "Set installation mode ACK: success=%s, mode=%u", YESNO(success), mode);
#ifdef USE_SELECT
      if (this->installation_mode_select_ != nullptr && success) {
        this->installation_mode_select_->publish_state(find_str(INSTALLATION_MODE_BY_UINT, mode));
      }
#endif
      break;
    }

    case CMD_QUERY_INSTALLATION_MODE: {
      uint8_t mode = this->buffer_data_[7];
      this->installation_mode_ = mode;
      ESP_LOGV(TAG, "Installation mode: %s", find_str(INSTALLATION_MODE_BY_UINT, mode));
#ifdef USE_SELECT
      if (this->installation_mode_select_ != nullptr) {
        this->installation_mode_select_->publish_state(find_str(INSTALLATION_MODE_BY_UINT, mode));
      }
#endif
      break;
    }

    case CMD_QUERY_VERSION: {
      uint8_t year = this->buffer_data_[8];
      uint8_t month = this->buffer_data_[9];
      uint8_t major = this->buffer_data_[10];
      uint8_t minor = this->buffer_data_[11];
      char version_s[32];
      snprintf(version_s, sizeof(version_s), "V%u.%u (20%02X-%02X)", major, minor, year, month);
      ESP_LOGV(TAG, "Firmware version: %s", version_s);
#ifdef USE_TEXT_SENSOR
      if (this->version_text_sensor_ != nullptr) {
        this->version_text_sensor_->publish_state(version_s);
      }
#endif
      break;
    }

    case CMD_SET_BAUD_RATE: {
      bool success = this->buffer_data_[7] == 0x01;
      ESP_LOGV(TAG, "Baud rate change ACK: %s", success ? "Success" : "Failed");
#ifdef USE_SELECT
      if (this->baud_rate_select_ != nullptr && success) {
        auto baud = this->baud_rate_select_->current_option();
        ESP_LOGI(TAG, "Baud rate changed to %.*s; restart module to apply", static_cast<int>(baud.size()),
                 baud.c_str());
      }
#endif
      break;
    }

    case CMD_FACTORY_RESET: {
      bool success = this->buffer_data_[7] == 0x01;
      ESP_LOGV(TAG, "Factory reset ACK: %s", success ? "Success" : "Failed");
      if (success) {
        this->set_timeout(500, [this]() { this->read_all_info(); });
      }
      break;
    }

    case CMD_SET_DETECTION_RANGE: {
      bool success = this->buffer_data_[7] == 0x01;
      ESP_LOGV(TAG, "Set detection range ACK: %s", success ? "Success" : "Failed");
      break;
    }

    case CMD_QUERY_DETECTION_RANGE: {
      uint8_t dist_01 = this->buffer_data_[7];
      int16_t min_angle_01 = static_cast<int16_t>(this->buffer_data_[8] | (this->buffer_data_[9] << 8));
      int16_t max_angle_01 = static_cast<int16_t>(this->buffer_data_[10] | (this->buffer_data_[11] << 8));
      this->detection_distance_ = dist_01 / 10.0f;
      this->detection_angle_min_ = min_angle_01 / 10.0f;
      this->detection_angle_max_ = max_angle_01 / 10.0f;
      ESP_LOGV(TAG, "Detection range: dist=%.1fm, min_angle=%.1f deg, max_angle=%.1f deg", this->detection_distance_,
               this->detection_angle_min_, this->detection_angle_max_);
#ifdef USE_NUMBER
      if (this->detection_distance_number_ != nullptr) {
        this->detection_distance_number_->publish_state(this->detection_distance_);
      }
      if (this->detection_angle_min_number_ != nullptr) {
        this->detection_angle_min_number_->publish_state(this->detection_angle_min_);
      }
      if (this->detection_angle_max_number_ != nullptr) {
        this->detection_angle_max_number_->publish_state(this->detection_angle_max_);
      }
#endif
      break;
    }

    case CMD_SET_SENSITIVITY: {
      bool success = this->buffer_data_[7] == 0x01;
      ESP_LOGV(TAG, "Set sensitivity ACK: %s", success ? "Success" : "Failed");
      break;
    }

    case CMD_QUERY_SENSITIVITY: {
      uint8_t sens = this->buffer_data_[7];
      this->sensitivity_ = sens;
      ESP_LOGV(TAG, "Sensitivity: %s", find_str(SENSITIVITY_BY_UINT, sens));
#ifdef USE_SELECT
      if (this->sensitivity_select_ != nullptr) {
        this->sensitivity_select_->publish_state(find_str(SENSITIVITY_BY_UINT, sens));
      }
#endif
      break;
    }

    default:
      ESP_LOGW(TAG, "Unhandled ACK for command: %02X", cmd);
      break;
  }
  return true;
}

void LD2460Component::readline_(int readch) {
  if (readch < 0) {
    return;
  }

  if (this->buffer_pos_ < MAX_LINE_LENGTH - 1) {
    this->buffer_data_[this->buffer_pos_++] = readch;
    this->buffer_data_[this->buffer_pos_] = 0;
  } else {
    ESP_LOGW(TAG, "Max command length exceeded; ignoring");
    this->buffer_pos_ = 0;
    return;
  }

  if (this->buffer_pos_ < HEADER_FOOTER_SIZE) {
    return;
  }

  if (validate_header_footer(DATA_FRAME_FOOTER, &this->buffer_data_[this->buffer_pos_ - 4])) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
    char hex_buf[format_hex_pretty_size(MAX_LINE_LENGTH)];
    ESP_LOGV(TAG, "Handling Periodic Data: %s", format_hex_pretty_to(hex_buf, this->buffer_data_, this->buffer_pos_));
#endif
    this->handle_periodic_data_();
    this->buffer_pos_ = 0;
  } else if (validate_header_footer(CMD_FRAME_FOOTER, &this->buffer_data_[this->buffer_pos_ - 4])) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
    char hex_buf[format_hex_pretty_size(MAX_LINE_LENGTH)];
    ESP_LOGV(TAG, "Handling Ack Data: %s", format_hex_pretty_to(hex_buf, this->buffer_data_, this->buffer_pos_));
#endif
    if (this->handle_ack_data_()) {
      this->buffer_pos_ = 0;
    }
  }
}

void LD2460Component::query_version_() {
  uint8_t payload = 0x01;
  this->send_command_(CMD_QUERY_VERSION, &payload, 1);
}

void LD2460Component::query_installation_params_() {
  uint8_t payload = 0x01;
  this->send_command_(CMD_QUERY_INSTALLATION_PARAMS, &payload, 1);
}

void LD2460Component::query_installation_mode_() {
  uint8_t payload = 0x01;
  this->send_command_(CMD_QUERY_INSTALLATION_MODE, &payload, 1);
}

void LD2460Component::query_detection_range_() {
  uint8_t payload = 0x01;
  this->send_command_(CMD_QUERY_DETECTION_RANGE, &payload, 1);
}

void LD2460Component::query_sensitivity_() {
  uint8_t payload = 0x01;
  this->send_command_(CMD_QUERY_SENSITIVITY, &payload, 1);
}

void LD2460Component::restart() {
  uint8_t payload = 0x01;
  this->send_command_(CMD_RESTART, &payload, 1);
}

void LD2460Component::factory_reset() {
  uint8_t payload = 0x01;
  this->send_command_(CMD_FACTORY_RESET, &payload, 1);
}

void LD2460Component::set_reporting(bool enable) {
  uint8_t payload = enable ? 0x01 : 0x00;
  this->send_command_(CMD_SET_REPORTING, &payload, 1);
}

void LD2460Component::set_baud_rate(const char *state) {
  uint8_t baud_val = find_uint8(BAUD_RATES_BY_STR, state);
  if (baud_val != 0xFF) {
    this->send_command_(CMD_SET_BAUD_RATE, &baud_val, 1);
  }
}

void LD2460Component::set_installation_mode(const char *state) {
  uint8_t mode = find_uint8(INSTALLATION_MODE_BY_STR, state);
  if (mode != 0xFF) {
    this->send_command_(CMD_SET_INSTALLATION_MODE, &mode, 1);
  }
}

void LD2460Component::set_sensitivity(const char *state) {
  uint8_t sens = find_uint8(SENSITIVITY_BY_STR, state);
  if (sens != 0xFF) {
    this->send_command_(CMD_SET_SENSITIVITY, &sens, 1);
  }
}

void LD2460Component::set_installation_params(float height, float angle) {
  this->installation_height_ = height;
  this->installation_angle_ = angle;
  uint16_t height_cm = static_cast<uint16_t>(roundf(height * 100.0f));
  uint16_t angle_x100 = static_cast<uint16_t>(roundf(angle * 100.0f));
  uint8_t payload[4] = {
      static_cast<uint8_t>(height_cm & 0xFF),
      static_cast<uint8_t>((height_cm >> 8) & 0xFF),
      static_cast<uint8_t>(angle_x100 & 0xFF),
      static_cast<uint8_t>((angle_x100 >> 8) & 0xFF),
  };
  this->send_command_(CMD_SET_INSTALLATION_PARAMS, payload, sizeof(payload));
}

void LD2460Component::set_detection_range(float distance, float min_angle, float max_angle) {
  this->detection_distance_ = distance;
  this->detection_angle_min_ = min_angle;
  this->detection_angle_max_ = max_angle;
  uint8_t dist_01 = static_cast<uint8_t>(roundf(distance * 10.0f));
  int16_t min_a01 = static_cast<int16_t>(roundf(min_angle * 10.0f));
  int16_t max_a01 = static_cast<int16_t>(roundf(max_angle * 10.0f));
  uint8_t payload[5] = {
      dist_01,
      static_cast<uint8_t>(min_a01 & 0xFF),
      static_cast<uint8_t>((min_a01 >> 8) & 0xFF),
      static_cast<uint8_t>(max_a01 & 0xFF),
      static_cast<uint8_t>((max_a01 >> 8) & 0xFF),
  };
  this->send_command_(CMD_SET_DETECTION_RANGE, payload, sizeof(payload));
}

}  // namespace esphome::ld2460

#include "ld6002b.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace esphome::ld6002b {

static const char *const TAG = "ld6002b";

static constexpr uint8_t TF_SOF = 0x01;
static constexpr uint32_t SETUP_DELAY_MS = 100;

// Command/message types
static constexpr uint16_t TYPE_CONTROL = 0x0201;
static constexpr uint16_t TYPE_SET_AREA = 0x0202;
static constexpr uint16_t TYPE_SET_HOLD_DELAY = 0x0203;
static constexpr uint16_t TYPE_SET_Z_RANGE = 0x0204;
static constexpr uint16_t TYPE_SET_LOW_POWER_SLEEP = 0x0205;

static constexpr uint16_t TYPE_REPORT_TARGET = 0x0A04;
static constexpr uint16_t TYPE_REPORT_POINT_CLOUD = 0x0A08;
static constexpr uint16_t TYPE_REPORT_AREA_PRESENCE = 0x0A0A;
static constexpr uint16_t TYPE_REPORT_INTERFERENCE_AREAS = 0x0A0B;
static constexpr uint16_t TYPE_REPORT_DETECTION_AREAS = 0x0A0C;
static constexpr uint16_t TYPE_REPORT_DELAY = 0x0A0D;
static constexpr uint16_t TYPE_REPORT_SENSITIVITY = 0x0A0E;
static constexpr uint16_t TYPE_REPORT_TRIGGER = 0x0A0F;
static constexpr uint16_t TYPE_REPORT_Z_RANGE = 0x0A10;
static constexpr uint16_t TYPE_REPORT_INSTALLATION = 0x0A11;
static constexpr uint16_t TYPE_REPORT_LOW_POWER = 0x0A12;
static constexpr uint16_t TYPE_REPORT_LOW_POWER_SLEEP = 0x0A13;
static constexpr uint16_t TYPE_REPORT_WORK_MODE = 0x0A14;
static constexpr uint16_t TYPE_QUERY_VERSION = 0xFFFF;

// Control command values for TYPE_CONTROL
static constexpr uint32_t CMD_AUTO_INTERFERENCE = 0x01;
static constexpr uint32_t CMD_GET_AREAS = 0x02;
static constexpr uint32_t CMD_CLEAR_INTERFERENCE = 0x03;
static constexpr uint32_t CMD_RESET_DETECTION_AREA = 0x04;
static constexpr uint32_t CMD_GET_DELAY = 0x05;
static constexpr uint32_t CMD_POINT_CLOUD_ON = 0x06;
static constexpr uint32_t CMD_POINT_CLOUD_OFF = 0x07;
static constexpr uint32_t CMD_TARGET_DISPLAY_ON = 0x08;
static constexpr uint32_t CMD_TARGET_DISPLAY_OFF = 0x09;
static constexpr uint32_t CMD_SENSITIVITY_LOW = 0x0A;
static constexpr uint32_t CMD_SENSITIVITY_MEDIUM = 0x0B;
static constexpr uint32_t CMD_SENSITIVITY_HIGH = 0x0C;
static constexpr uint32_t CMD_GET_SENSITIVITY = 0x0D;
static constexpr uint32_t CMD_TRIGGER_SLOW = 0x0E;
static constexpr uint32_t CMD_TRIGGER_MEDIUM = 0x0F;
static constexpr uint32_t CMD_TRIGGER_FAST = 0x10;
static constexpr uint32_t CMD_GET_TRIGGER = 0x11;
static constexpr uint32_t CMD_GET_Z_RANGE = 0x12;
static constexpr uint32_t CMD_INSTALL_TOP = 0x13;
static constexpr uint32_t CMD_INSTALL_SIDE = 0x14;
static constexpr uint32_t CMD_GET_INSTALLATION = 0x15;
static constexpr uint32_t CMD_LOW_POWER_ON = 0x16;
static constexpr uint32_t CMD_LOW_POWER_OFF = 0x17;
static constexpr uint32_t CMD_GET_LOW_POWER = 0x18;
static constexpr uint32_t CMD_GET_LOW_POWER_SLEEP = 0x19;
static constexpr uint32_t CMD_RESET_UNATTENDED = 0x1A;

static constexpr uint16_t TARGET_DATA_LEN = 20;  // x,y,z,dop_idx,cluster_id
static constexpr uint16_t AREA_DATA_LEN = 24;    // 6 floats
static constexpr uint16_t AREA_CONFIG_LEN = 28;  // int32 + 6 floats

static constexpr uint8_t AREA_ID_DEFAULT = 4;  // detection_0 for initial display

static constexpr uint8_t VERSION_QUERY_DATA[] = {0x01, 0x01, 0x00, 0x00};

static uint32_t read_control_command_value(const uint8_t *data, uint8_t len) {
  if (data == nullptr || len < 4) {
    return 0;
  }

  return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

#ifdef ESPHOME_LOG_HAS_VERBOSE
static const char *control_command_name(uint32_t command) {
  switch (command) {
    case CMD_AUTO_INTERFERENCE:
      return "auto_interference";
    case CMD_GET_AREAS:
      return "get_areas";
    case CMD_CLEAR_INTERFERENCE:
      return "clear_interference";
    case CMD_RESET_DETECTION_AREA:
      return "reset_detection_area";
    case CMD_GET_DELAY:
      return "get_delay";
    case CMD_POINT_CLOUD_ON:
      return "point_cloud_on";
    case CMD_POINT_CLOUD_OFF:
      return "point_cloud_off";
    case CMD_TARGET_DISPLAY_ON:
      return "target_display_on";
    case CMD_TARGET_DISPLAY_OFF:
      return "target_display_off";
    case CMD_SENSITIVITY_LOW:
      return "sensitivity_low";
    case CMD_SENSITIVITY_MEDIUM:
      return "sensitivity_medium";
    case CMD_SENSITIVITY_HIGH:
      return "sensitivity_high";
    case CMD_GET_SENSITIVITY:
      return "get_sensitivity";
    case CMD_TRIGGER_SLOW:
      return "trigger_slow";
    case CMD_TRIGGER_MEDIUM:
      return "trigger_medium";
    case CMD_TRIGGER_FAST:
      return "trigger_fast";
    case CMD_GET_TRIGGER:
      return "get_trigger";
    case CMD_GET_Z_RANGE:
      return "get_z_range";
    case CMD_INSTALL_TOP:
      return "install_top";
    case CMD_INSTALL_SIDE:
      return "install_side";
    case CMD_GET_INSTALLATION:
      return "get_installation";
    case CMD_LOW_POWER_ON:
      return "low_power_on";
    case CMD_LOW_POWER_OFF:
      return "low_power_off";
    case CMD_GET_LOW_POWER:
      return "get_low_power";
    case CMD_GET_LOW_POWER_SLEEP:
      return "get_low_power_sleep";
    case CMD_RESET_UNATTENDED:
      return "reset_unattended";
    default:
      return "unknown";
  }
}

static const char *frame_type_name(uint16_t type) {
  switch (type) {
    case TYPE_CONTROL:
      return "control";
    case TYPE_SET_AREA:
      return "set_area";
    case TYPE_SET_HOLD_DELAY:
      return "set_hold_delay";
    case TYPE_SET_Z_RANGE:
      return "set_z_range";
    case TYPE_SET_LOW_POWER_SLEEP:
      return "set_low_power_sleep";
    case TYPE_REPORT_TARGET:
      return "report_target";
    case TYPE_REPORT_POINT_CLOUD:
      return "report_point_cloud";
    case TYPE_REPORT_AREA_PRESENCE:
      return "report_area_presence";
    case TYPE_REPORT_INTERFERENCE_AREAS:
      return "report_interference_areas";
    case TYPE_REPORT_DETECTION_AREAS:
      return "report_detection_areas";
    case TYPE_REPORT_DELAY:
      return "report_delay";
    case TYPE_REPORT_SENSITIVITY:
      return "report_sensitivity";
    case TYPE_REPORT_TRIGGER:
      return "report_trigger";
    case TYPE_REPORT_Z_RANGE:
      return "report_z_range";
    case TYPE_REPORT_INSTALLATION:
      return "report_installation";
    case TYPE_REPORT_LOW_POWER:
      return "report_low_power";
    case TYPE_REPORT_LOW_POWER_SLEEP:
      return "report_low_power_sleep";
    case TYPE_REPORT_WORK_MODE:
      return "report_work_mode";
    case TYPE_QUERY_VERSION:
      return "query_version";
    default:
      return "unknown";
  }
}

static bool is_expected_control_report(uint32_t command, uint16_t type) {
  switch (command) {
    case CMD_GET_AREAS:
      return type == TYPE_REPORT_INTERFERENCE_AREAS || type == TYPE_REPORT_DETECTION_AREAS;
    case CMD_GET_DELAY:
      return type == TYPE_REPORT_DELAY;
    case CMD_GET_SENSITIVITY:
      return type == TYPE_REPORT_SENSITIVITY;
    case CMD_GET_TRIGGER:
      return type == TYPE_REPORT_TRIGGER;
    case CMD_GET_Z_RANGE:
      return type == TYPE_REPORT_Z_RANGE;
    case CMD_GET_INSTALLATION:
      return type == TYPE_REPORT_INSTALLATION;
    case CMD_GET_LOW_POWER:
    case CMD_LOW_POWER_ON:
    case CMD_LOW_POWER_OFF:
      return type == TYPE_REPORT_LOW_POWER;
    case CMD_GET_LOW_POWER_SLEEP:
      return type == TYPE_REPORT_LOW_POWER_SLEEP;
    default:
      return false;
  }
}
#endif

uint16_t LD6002BComponent::read_u16_be(const uint8_t *data) { return (static_cast<uint16_t>(data[0]) << 8) | data[1]; }

uint32_t LD6002BComponent::read_u32_le(const uint8_t *data) {
  return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

int32_t LD6002BComponent::read_int32_le(const uint8_t *data) {
  uint32_t raw = read_u32_le(data);
  int32_t value;
  std::memcpy(&value, &raw, sizeof(value));
  return value;
}

float LD6002BComponent::read_f32_le(const uint8_t *data) {
  uint32_t raw = read_u32_le(data);
  float value;
  std::memcpy(&value, &raw, sizeof(value));
  return value;
}

bool LD6002BComponent::should_publish_float(float previous, float next, float epsilon) {
  if (std::isnan(previous) && std::isnan(next)) {
    return false;
  }
  if (std::isnan(previous) != std::isnan(next)) {
    return true;
  }
  return std::fabs(previous - next) > epsilon;
}

void LD6002BComponent::write_u32_le(uint8_t *data, uint32_t value) {
  data[0] = value & 0xFF;
  data[1] = (value >> 8) & 0xFF;
  data[2] = (value >> 16) & 0xFF;
  data[3] = (value >> 24) & 0xFF;
}

void LD6002BComponent::write_int32_le(uint8_t *data, int32_t value) {
  write_u32_le(data, static_cast<uint32_t>(value));
}

void LD6002BComponent::write_f32_le(uint8_t *data, float value) {
  uint32_t raw;
  std::memcpy(&raw, &value, sizeof(raw));
  write_u32_le(data, raw);
}

void LD6002BComponent::set_max_data_len(size_t max_data_len) {
  this->max_data_len_overridden_ = true;
  this->max_data_len_ = max_data_len;
  this->data_buf_ = std::make_unique<uint8_t[]>(std::max<size_t>(max_data_len, 6));
}

void LD6002BComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up HLK-LD6002B...");
  size_t desired_data_len = this->max_data_len_;
  if (!this->max_data_len_overridden_) {
    bool point_cloud_configured = false;
#ifdef USE_SENSOR
    point_cloud_configured = point_cloud_configured || this->point_count_sensor_ != nullptr;
#endif
#ifdef USE_SWITCH
    point_cloud_configured = point_cloud_configured || this->point_cloud_switch_ != nullptr;
#endif
    desired_data_len = point_cloud_configured ? DEFAULT_MAX_DATA_LEN_POINT_CLOUD : DEFAULT_MAX_DATA_LEN;
  }
  this->max_data_len_ = desired_data_len;
  this->data_buf_ = std::make_unique<uint8_t[]>(std::max<size_t>(desired_data_len, 6));
  if (this->wakeup_pin_ != nullptr) {
    this->wakeup_pin_->setup();
    this->wakeup_pin_->digital_write(true);
  }

  this->set_timeout(SETUP_DELAY_MS, [this]() {
    bool want_target_stream = false;
#ifdef USE_SENSOR
    want_target_stream = want_target_stream || this->target_count_sensor_ != nullptr;
    if (!want_target_stream) {
      for (const auto &target : this->targets_) {
        if (target.x != nullptr || target.y != nullptr || target.z != nullptr || target.dop_idx != nullptr ||
            target.cluster_id != nullptr) {
          want_target_stream = true;
          break;
        }
      }
    }
#endif
#ifdef USE_BINARY_SENSOR
    want_target_stream = want_target_stream || this->presence_binary_sensor_ != nullptr;
    if (!want_target_stream) {
      for (auto *sensor : this->target_presence_) {
        if (sensor != nullptr) {
          want_target_stream = true;
          break;
        }
      }
    }
#endif
    if (want_target_stream) {
      this->send_control_command_(CMD_TARGET_DISPLAY_ON);
#ifdef USE_SWITCH
      if (this->target_display_switch_ != nullptr) {
        this->target_display_switch_->publish_state(true);
      }
#endif
    }

    bool want_point_cloud_stream = false;
#ifdef USE_SENSOR
    want_point_cloud_stream = want_point_cloud_stream || this->point_count_sensor_ != nullptr;
#endif
    this->send_control_command_(want_point_cloud_stream ? CMD_POINT_CLOUD_ON : CMD_POINT_CLOUD_OFF);
#ifdef USE_SWITCH
    if (this->point_cloud_switch_ != nullptr) {
      this->point_cloud_switch_->publish_state(want_point_cloud_stream);
    }
#endif

#ifdef USE_SELECT
    if (this->sensitivity_select_ != nullptr) {
      this->send_control_command_(CMD_GET_SENSITIVITY);
    }
    if (this->trigger_speed_select_ != nullptr) {
      this->send_control_command_(CMD_GET_TRIGGER);
    }
    if (this->installation_select_ != nullptr) {
      this->send_control_command_(CMD_GET_INSTALLATION);
    }
#endif
#ifdef USE_NUMBER
    if (this->z_min_number_ != nullptr || this->z_max_number_ != nullptr) {
      this->send_control_command_(CMD_GET_Z_RANGE);
    }
    if (this->low_power_sleep_number_ != nullptr) {
      this->send_control_command_(CMD_GET_LOW_POWER_SLEEP);
    }
    if (this->hold_delay_number_ != nullptr) {
      this->send_control_command_(CMD_GET_DELAY);
    }
#endif
#ifdef USE_SWITCH
    bool want_low_power = this->low_power_switch_ != nullptr;
#else
    bool want_low_power = false;
#endif
#ifdef USE_TEXT_SENSOR
    want_low_power = want_low_power || this->work_mode_text_sensor_ != nullptr;
#endif
    if (want_low_power) {
      this->send_control_command_(CMD_GET_LOW_POWER);
    }

    bool want_area_report = false;
#ifdef USE_SENSOR
    for (const auto &area : this->interference_areas_) {
      if (area.x_min != nullptr || area.x_max != nullptr || area.y_min != nullptr || area.y_max != nullptr ||
          area.z_min != nullptr || area.z_max != nullptr) {
        want_area_report = true;
        break;
      }
    }
    if (!want_area_report) {
      for (const auto &area : this->detection_areas_) {
        if (area.x_min != nullptr || area.x_max != nullptr || area.y_min != nullptr || area.y_max != nullptr ||
            area.z_min != nullptr || area.z_max != nullptr) {
          want_area_report = true;
          break;
        }
      }
    }
#endif
#ifdef USE_NUMBER
    if (this->area_x_min_number_ != nullptr || this->area_x_max_number_ != nullptr ||
        this->area_y_min_number_ != nullptr || this->area_y_max_number_ != nullptr ||
        this->area_z_min_number_ != nullptr || this->area_z_max_number_ != nullptr) {
      want_area_report = true;
    }
#endif
    if (want_area_report) {
      this->send_control_command_(CMD_GET_AREAS);
    }

    this->init_installation_pref_();
    this->init_area_id_pref_();
    this->init_version_pref_();

#ifdef USE_TEXT_SENSOR
    if (this->ota_version_text_sensor_ != nullptr) {
      this->send_command_untracked_(TYPE_QUERY_VERSION, VERSION_QUERY_DATA, sizeof(VERSION_QUERY_DATA));
    }
#endif
  });
}

void LD6002BComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HLK-LD6002B:");
  ESP_LOGCONFIG(TAG, "  Auto wake: %s", this->auto_wake_ ? "true" : "false");
  if (this->throttle_ms_ > 0) {
    ESP_LOGCONFIG(TAG, "  Throttle: %ums", this->throttle_ms_);
  }
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Target Count", this->target_count_sensor_);
  LOG_SENSOR("  ", "Point Count", this->point_count_sensor_);
#endif
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Presence", this->presence_binary_sensor_);
#endif
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Work Mode", this->work_mode_text_sensor_);
  LOG_TEXT_SENSOR("  ", "OTA Version", this->ota_version_text_sensor_);
#endif
}

void LD6002BComponent::loop() {
  while (this->available()) {
    uint8_t byte = this->read();
    this->parse_byte_(byte);
  }
  this->process_command_queue_();
}

bool LD6002BComponent::should_throttle_stream_(uint32_t &last_publish_ms) {
  if (this->throttle_ms_ == 0) {
    return false;
  }
  uint32_t now = millis();
  if (last_publish_ms == 0) {
    last_publish_ms = now;
    return false;
  }
  if (now - last_publish_ms < this->throttle_ms_) {
    return true;
  }
  last_publish_ms = now;
  return false;
}

void LD6002BComponent::init_installation_pref_() {
#ifdef USE_SELECT
  if (this->installation_select_ == nullptr) {
    return;
  }
  this->installation_pref_ = this->installation_select_->make_entity_preference<uint8_t>();
  this->installation_pref_initialized_ = true;

  uint8_t value = 0;
  if (this->installation_pref_.load(&value) && value <= 1) {
    this->installation_select_->publish_state(value);
  }
#endif
}

void LD6002BComponent::save_installation_pref_(uint8_t value) {
#ifdef USE_SELECT
  if (!this->installation_pref_initialized_) {
    return;
  }
  this->installation_pref_.save(&value);
#endif
}

void LD6002BComponent::reset_parser_() {
  this->parse_state_ = ParseState::SOF;
  this->header_pos_ = 0;
  this->header_xor_ = 0;
  this->data_len_ = 0;
  this->data_pos_ = 0;
  this->data_xor_ = 0;
  this->discard_remaining_ = 0;
}

void LD6002BComponent::parse_byte_(uint8_t byte) {
  switch (this->parse_state_) {
    case ParseState::DISCARD:
      if (this->discard_remaining_ > 0) {
        this->discard_remaining_--;
        if (this->discard_remaining_ == 0) {
          this->reset_parser_();
        }
      } else {
        this->reset_parser_();
      }
      return;
    case ParseState::SOF:
      if (byte != TF_SOF)
        return;
      this->header_pos_ = 0;
      this->header_xor_ = 0;
      this->header_xor_ ^= byte;
      this->parse_state_ = ParseState::HEADER;
      return;
    case ParseState::HEADER:
      if (this->header_pos_ < 6) {
        this->data_buf_[this->header_pos_] = byte;
        this->header_xor_ ^= byte;
        this->header_pos_++;
        if (this->header_pos_ == 6) {
          this->frame_id_ = read_u16_be(this->data_buf_.get());
          this->data_len_ = read_u16_be(this->data_buf_.get() + 2);
          this->frame_type_ = read_u16_be(this->data_buf_.get() + 4);
          if (this->data_len_ > this->max_data_len_) {
            ESP_LOGW(TAG, "Frame too large: %u", this->data_len_);
            // Discard the remaining bytes in this frame: header checksum + payload + data checksum (if any)
            this->discard_remaining_ = 1 + this->data_len_ + (this->data_len_ > 0 ? 1 : 0);
            this->parse_state_ = ParseState::DISCARD;
            return;
          }
          this->parse_state_ = ParseState::HCK;
        }
      }
      return;
    case ParseState::HCK: {
      uint8_t expected = static_cast<uint8_t>(~this->header_xor_);
      if (byte != expected) {
        ESP_LOGV(TAG, "Header checksum mismatch");
        this->reset_parser_();
        return;
      }
      if (this->data_len_ == 0) {
        this->handle_frame_(this->frame_type_, nullptr, 0);
        this->reset_parser_();
      } else {
        this->data_pos_ = 0;
        this->data_xor_ = 0;
        this->parse_state_ = ParseState::DATA;
      }
      return;
    }
    case ParseState::DATA:
      this->data_buf_[this->data_pos_++] = byte;
      this->data_xor_ ^= byte;
      if (this->data_pos_ >= this->data_len_) {
        this->parse_state_ = ParseState::DCK;
      }
      return;
    case ParseState::DCK: {
      uint8_t expected = static_cast<uint8_t>(~this->data_xor_);
      if (byte == expected) {
        this->handle_frame_(this->frame_type_, this->data_buf_.get(), this->data_len_);
      } else {
        ESP_LOGV(TAG, "Data checksum mismatch");
      }
      this->reset_parser_();
      return;
    }
  }
}

void LD6002BComponent::handle_frame_(uint16_t type, const uint8_t *data, uint16_t len) {
  const bool matching_ack_type = len == 0 && this->command_active_ && type == this->active_command_.type;
  const bool matching_ack_frame_id =
      ((this->frame_id_ & 0x7FFF) == (this->active_frame_id_ & 0x7FFF)) || this->active_command_.type == TYPE_CONTROL;
  const bool matching_ack = matching_ack_type && matching_ack_frame_id;
  if (matching_ack) {
#ifdef ESPHOME_LOG_HAS_VERBOSE
    const uint32_t active_control_command =
        this->active_command_.type == TYPE_CONTROL
            ? read_control_command_value(this->active_command_.data.data(), this->active_command_.len)
            : 0;
    if (active_control_command != 0 && ((this->frame_id_ & 0x7FFF) != (this->active_frame_id_ & 0x7FFF))) {
      ESP_LOGV(TAG, "Accepting %s (0x%02" PRIX32 ") ACK with device frame 0x%04X while active frame is 0x%04X",
               control_command_name(active_control_command), active_control_command, this->frame_id_,
               this->active_frame_id_);
    }
    if (active_control_command != 0) {
      ESP_LOGV(TAG, "ACK for %s (0x%02" PRIX32 ") matched frame 0x%04X", control_command_name(active_control_command),
               active_control_command, this->frame_id_);
    }
#endif
    const bool refresh_areas = (type == TYPE_SET_AREA) && this->area_write_pending_;
    this->command_active_ = false;
    this->command_sent_ = false;
    this->active_frame_id_ = 0;
    this->last_send_ms_ = 0;
    this->process_command_queue_();
    if (refresh_areas) {
      this->area_write_pending_ = false;
      this->set_timeout(50, [this]() { this->send_control_command_(CMD_GET_AREAS); });
    }
    return;
  }

#ifdef ESPHOME_LOG_HAS_VERBOSE
  const uint32_t active_control_command =
      this->command_active_ && this->active_command_.type == TYPE_CONTROL
          ? read_control_command_value(this->active_command_.data.data(), this->active_command_.len)
          : 0;
  if (len == 0 && this->command_active_ && type == this->active_command_.type &&
      this->frame_id_ != this->active_frame_id_ && this->active_command_.type != TYPE_CONTROL) {
    ESP_LOGV(TAG, "Ignoring ACK for %s (0x%02" PRIX32 "): frame 0x%04X did not match active 0x%04X",
             control_command_name(active_control_command), active_control_command, this->frame_id_,
             this->active_frame_id_);
  }

  if (active_control_command != 0 && is_expected_control_report(active_control_command, type)) {
    ESP_LOGV(TAG, "Received %s (0x%04X) while waiting for %s (0x%02" PRIX32 ") ACK", frame_type_name(type), type,
             control_command_name(active_control_command), active_control_command);
  }
#endif

  switch (type) {
    case TYPE_REPORT_TARGET:
      this->handle_target_report_(data, len);
      break;
    case TYPE_REPORT_POINT_CLOUD:
      if (this->should_throttle_stream_(this->last_point_publish_)) {
        return;
      }
      this->handle_point_cloud_(data, len);
      break;
    case TYPE_REPORT_AREA_PRESENCE:
      this->handle_area_presence_(data, len);
      break;
    case TYPE_REPORT_INTERFERENCE_AREAS:
      this->handle_area_report_(true, data, len);
      break;
    case TYPE_REPORT_DETECTION_AREAS:
      this->handle_area_report_(false, data, len);
      break;
    case TYPE_REPORT_DELAY:
      this->handle_delay_report_(data, len);
      break;
    case TYPE_REPORT_SENSITIVITY:
      this->handle_sensitivity_report_(data, len);
      break;
    case TYPE_REPORT_TRIGGER:
      this->handle_trigger_speed_report_(data, len);
      break;
    case TYPE_REPORT_Z_RANGE:
      this->handle_z_range_report_(data, len);
      break;
    case TYPE_REPORT_INSTALLATION:
      this->handle_installation_report_(data, len);
      break;
    case TYPE_REPORT_LOW_POWER:
      this->handle_low_power_report_(data, len);
      break;
    case TYPE_REPORT_LOW_POWER_SLEEP:
      this->handle_low_power_sleep_report_(data, len);
      break;
    case TYPE_REPORT_WORK_MODE:
      this->handle_work_mode_report_(data, len);
      break;
    case TYPE_QUERY_VERSION:
      this->handle_version_report_(data, len);
      break;
    default:
      break;
  }
}

void LD6002BComponent::handle_target_report_(const uint8_t *data, uint16_t len) {
  if (len < 4)
    return;

  // Throttle only the high-churn coordinate payloads. Presence/count transitions
  // should still propagate immediately so entities do not look stale.
  const bool publish_target_values = !this->should_throttle_stream_(this->last_target_publish_);

  uint32_t target_num = read_u32_le(data);
  uint16_t available = (len - 4) / TARGET_DATA_LEN;
  uint8_t count = static_cast<uint8_t>(std::min<uint32_t>(target_num, available));

#ifdef USE_SENSOR
  if (this->target_count_sensor_ != nullptr) {
    if (target_num != this->last_target_count_) {
      this->target_count_sensor_->publish_state(target_num);
      this->last_target_count_ = target_num;
    }
  }
#endif

  this->target_presence_any_ = (count > 0);
  bool presence = this->target_presence_any_ || this->area_presence_any_;
#ifdef USE_BINARY_SENSOR
  if (this->presence_binary_sensor_ != nullptr) {
    this->presence_binary_sensor_->publish_state(presence);
  }
#endif
  this->update_work_mode_fallback_();

  for (uint8_t i = 0; i < MAX_TARGETS; i++) {
    bool has_target = i < count;
    if (has_target) {
      uint16_t offset = 4 + (i * TARGET_DATA_LEN);
      float x = read_f32_le(data + offset + 0);
      float y = read_f32_le(data + offset + 4);
      float z = read_f32_le(data + offset + 8);
      int32_t dop_idx = read_int32_le(data + offset + 12);
      int32_t cluster_id = static_cast<int32_t>(read_u32_le(data + offset + 16));
#ifdef USE_SENSOR
      TargetSensors &target = this->targets_[i];
      if (publish_target_values && target.x != nullptr && should_publish_float(this->last_target_x_[i], x)) {
        target.x->publish_state(x);
        this->last_target_x_[i] = x;
      }
      if (publish_target_values && target.y != nullptr && should_publish_float(this->last_target_y_[i], y)) {
        target.y->publish_state(y);
        this->last_target_y_[i] = y;
      }
      if (publish_target_values && target.z != nullptr && should_publish_float(this->last_target_z_[i], z)) {
        target.z->publish_state(z);
        this->last_target_z_[i] = z;
      }
      float dop_value = static_cast<float>(dop_idx);
      if (publish_target_values && target.dop_idx != nullptr &&
          should_publish_float(this->last_target_dop_[i], dop_value)) {
        target.dop_idx->publish_state(dop_value);
        this->last_target_dop_[i] = dop_value;
      }
      float cluster_value = static_cast<float>(cluster_id);
      if (publish_target_values && target.cluster_id != nullptr &&
          should_publish_float(this->last_target_cluster_[i], cluster_value)) {
        target.cluster_id->publish_state(cluster_value);
        this->last_target_cluster_[i] = cluster_value;
      }
#endif
    } else {
#ifdef USE_SENSOR
      TargetSensors &target = this->targets_[i];
      if (this->last_target_presence_[i]) {
        if (target.x != nullptr) {
          target.x->publish_state(NAN);
          this->last_target_x_[i] = NAN;
        }
        if (target.y != nullptr) {
          target.y->publish_state(NAN);
          this->last_target_y_[i] = NAN;
        }
        if (target.z != nullptr) {
          target.z->publish_state(NAN);
          this->last_target_z_[i] = NAN;
        }
        if (target.dop_idx != nullptr) {
          target.dop_idx->publish_state(NAN);
          this->last_target_dop_[i] = NAN;
        }
        if (target.cluster_id != nullptr) {
          target.cluster_id->publish_state(NAN);
          this->last_target_cluster_[i] = NAN;
        }
      }
#endif
    }
#ifdef USE_BINARY_SENSOR
    if (this->target_presence_[i] != nullptr) {
      if (!this->target_presence_initialized_[i] || has_target != this->last_target_presence_[i]) {
        this->target_presence_[i]->publish_state(has_target);
        this->target_presence_initialized_[i] = true;
      }
    }
#endif
    this->last_target_presence_[i] = has_target;
  }
}

void LD6002BComponent::handle_point_cloud_(const uint8_t *data, uint16_t len) {
  if (len < 4)
    return;

  uint32_t point_num = read_u32_le(data);

#ifdef USE_SENSOR
  if (this->point_count_sensor_ != nullptr) {
    if (point_num != this->last_point_count_) {
      this->point_count_sensor_->publish_state(point_num);
      this->last_point_count_ = point_num;
    }
  }
#endif
}

void LD6002BComponent::handle_area_presence_(const uint8_t *data, uint16_t len) {
  if (len < 16)
    return;

  this->area_presence_any_ = false;
  for (uint8_t i = 0; i < AREA_COUNT; i++) {
    uint32_t state = read_u32_le(data + (i * 4));
    bool present = state != 0;
    this->area_presence_any_ = this->area_presence_any_ || present;
#ifdef USE_BINARY_SENSOR
    if (this->area_presence_[i] != nullptr) {
      this->area_presence_[i]->publish_state(present);
    }
#endif
  }

  bool presence = this->target_presence_any_ || this->area_presence_any_;
#ifdef USE_BINARY_SENSOR
  if (this->presence_binary_sensor_ != nullptr) {
    this->presence_binary_sensor_->publish_state(presence);
  }
#endif
  this->update_work_mode_fallback_();
}

void LD6002BComponent::handle_area_report_(bool interference, const uint8_t *data, uint16_t len) {
  uint16_t needed = AREA_COUNT * AREA_DATA_LEN;
  if (len < needed)
    return;

  for (uint8_t i = 0; i < AREA_COUNT; i++) {
    uint16_t offset = i * AREA_DATA_LEN;
    float x_min = read_f32_le(data + offset + 0);
    float x_max = read_f32_le(data + offset + 4);
    float y_min = read_f32_le(data + offset + 8);
    float y_max = read_f32_le(data + offset + 12);
    float z_min = read_f32_le(data + offset + 16);
    float z_max = read_f32_le(data + offset + 20);

#ifdef USE_SENSOR
    AreaSensors &area = interference ? this->interference_areas_[i] : this->detection_areas_[i];
    if (area.x_min != nullptr)
      area.x_min->publish_state(x_min);
    if (area.x_max != nullptr)
      area.x_max->publish_state(x_max);
    if (area.y_min != nullptr)
      area.y_min->publish_state(y_min);
    if (area.y_max != nullptr)
      area.y_max->publish_state(y_max);
    if (area.z_min != nullptr)
      area.z_min->publish_state(z_min);
    if (area.z_max != nullptr)
      area.z_max->publish_state(z_max);
#endif

    AreaConfig &store = interference ? this->interference_area_values_[i] : this->detection_area_values_[i];
    store.x_min = x_min;
    store.x_max = x_max;
    store.y_min = y_min;
    store.y_max = y_max;
    store.z_min = z_min;
    store.z_max = z_max;

    uint8_t selected_id = this->area_id_set_ ? this->area_id_ : AREA_ID_DEFAULT;
    bool selected_interference = selected_id < AREA_COUNT;
    uint8_t selected_index = selected_interference ? selected_id : static_cast<uint8_t>(selected_id - AREA_COUNT);
    if (selected_interference == interference && selected_index == i) {
      this->update_area_numbers_(store);
    }
  }
  this->try_apply_pending_area_();
}

void LD6002BComponent::handle_delay_report_(const uint8_t *data, uint16_t len) {
  if (len < 4)
    return;
  uint32_t delay = read_u32_le(data);
  this->hold_delay_seconds_ = delay;
#ifdef USE_NUMBER
  if (this->hold_delay_number_ != nullptr) {
    this->hold_delay_number_->publish_state(delay);
  }
#endif
}

void LD6002BComponent::handle_sensitivity_report_(const uint8_t *data, uint16_t len) {
  if (len < 1)
    return;
#ifdef USE_SELECT
  if (this->sensitivity_select_ == nullptr)
    return;
  uint8_t value = data[0];
  if (value <= 2) {
    this->sensitivity_select_->publish_state(value);
  }
#endif
}

void LD6002BComponent::handle_trigger_speed_report_(const uint8_t *data, uint16_t len) {
  if (len < 1)
    return;
#ifdef USE_SELECT
  if (this->trigger_speed_select_ == nullptr)
    return;
  uint8_t value = data[0];
  if (value <= 2) {
    this->trigger_speed_select_->publish_state(value);
  }
#endif
}

void LD6002BComponent::handle_z_range_report_(const uint8_t *data, uint16_t len) {
  if (len < 8)
    return;
  float z_min = read_f32_le(data);
  float z_max = read_f32_le(data + 4);
  this->z_min_ = z_min;
  this->z_max_ = z_max;
#ifdef USE_NUMBER
  if (this->z_min_number_ != nullptr) {
    this->z_min_number_->publish_state(z_min);
  }
  if (this->z_max_number_ != nullptr) {
    this->z_max_number_->publish_state(z_max);
  }
#endif
}

void LD6002BComponent::handle_installation_report_(const uint8_t *data, uint16_t len) {
  if (len < 1)
    return;
#ifdef USE_SELECT
  if (this->installation_select_ == nullptr)
    return;
  uint8_t value = data[0];
  if (value <= 1) {
    this->installation_select_->publish_state(value);
    this->save_installation_pref_(value);
  }
#endif
}

void LD6002BComponent::handle_low_power_report_(const uint8_t *data, uint16_t len) {
  if (len < 1)
    return;
  bool enabled = data[0] != 0;
  this->low_power_enabled_ = enabled;
  this->low_power_reported_ = true;
#ifdef USE_SWITCH
  if (this->low_power_switch_ != nullptr) {
    this->low_power_switch_->publish_state(enabled);
  }
#endif
#ifdef USE_TEXT_SENSOR
  if (this->work_mode_text_sensor_ != nullptr && !this->work_mode_reported_) {
    this->publish_work_mode_(enabled);
  }
#endif
  this->update_work_mode_fallback_();
}

void LD6002BComponent::handle_low_power_sleep_report_(const uint8_t *data, uint16_t len) {
  if (len < 4)
    return;
  uint32_t sleep_ms = read_u32_le(data);
#ifdef USE_NUMBER
  if (this->low_power_sleep_number_ != nullptr) {
    this->low_power_sleep_number_->publish_state(sleep_ms);
  }
#endif
}

void LD6002BComponent::handle_work_mode_report_(const uint8_t *data, uint16_t len) {
  if (len < 1)
    return;
  const bool low_power = (data[0] == 0);
#ifdef USE_TEXT_SENSOR
  if (this->work_mode_text_sensor_ != nullptr) {
    this->work_mode_reported_ = true;
    this->publish_work_mode_(low_power);
  }
#endif
}

void LD6002BComponent::update_work_mode_fallback_() {
#ifdef USE_TEXT_SENSOR
  if (this->work_mode_text_sensor_ == nullptr || this->work_mode_reported_) {
    return;
  }
  if (!this->low_power_reported_) {
    return;
  }
  const bool presence = this->target_presence_any_ || this->area_presence_any_;
  const char *mode = (!this->low_power_enabled_ || presence) ? "normal" : "low_power";
  this->publish_work_mode_(mode[0] == 'l');
#endif
}

void LD6002BComponent::publish_work_mode_(bool low_power) {
#ifdef USE_TEXT_SENSOR
  if (this->work_mode_text_sensor_ == nullptr) {
    return;
  }
  if (this->last_work_mode_valid_ && this->last_work_mode_low_power_ == low_power) {
    return;
  }
  this->work_mode_text_sensor_->publish_state(low_power ? "low_power" : "normal");
  this->last_work_mode_valid_ = true;
  this->last_work_mode_low_power_ = low_power;
#endif
}

void LD6002BComponent::handle_version_report_(const uint8_t *data, uint16_t len) {
  if (len < 4)
    return;
#ifdef USE_TEXT_SENSOR
  if (this->ota_version_text_sensor_ == nullptr)
    return;
  uint8_t project = data[0];
  uint8_t major = data[1];
  uint8_t minor = data[2];
  uint8_t patch = data[3];
  char buf[32];
  if (project == 0) {
    std::snprintf(buf, sizeof(buf), "%u.%u.%u", major, minor, patch);
  } else {
    std::snprintf(buf, sizeof(buf), "p%u %u.%u.%u", project, major, minor, patch);
  }
  this->ota_version_text_sensor_->publish_state(buf);
  this->save_version_pref_(buf);
#endif
}

void LD6002BComponent::queue_command_(uint16_t type, const uint8_t *data, uint8_t len) {
  if (len > CMD_MAX_DATA_LEN) {
    ESP_LOGW(TAG, "Command data too large: %u", len);
    return;
  }
  if (this->cmd_count_ >= CMD_QUEUE_SIZE) {
    ESP_LOGW(TAG, "Command queue full, dropping command 0x%04X", type);
    return;
  }

  PendingCommand &cmd = this->cmd_queue_[this->cmd_tail_];
  cmd.type = type;
  cmd.len = len;
  if (len > 0 && data != nullptr) {
    std::memcpy(cmd.data.data(), data, len);
  }

  this->cmd_tail_ = (this->cmd_tail_ + 1) % CMD_QUEUE_SIZE;
  this->cmd_count_++;
  this->process_command_queue_();
}

void LD6002BComponent::process_command_queue_() {
  uint32_t now = millis();
  if (this->command_active_) {
    if (this->command_sent_ && now - this->last_send_ms_ >= CMD_ACK_TIMEOUT_MS) {
      const uint32_t active_control_command =
          this->active_command_.type == TYPE_CONTROL
              ? read_control_command_value(this->active_command_.data.data(), this->active_command_.len)
              : 0;
      if (this->retries_left_ > 0) {
#ifdef ESPHOME_LOG_HAS_VERBOSE
        if (active_control_command != 0) {
          ESP_LOGV(TAG, "Retrying %s (0x%02" PRIX32 "), %u attempt(s) remaining",
                   control_command_name(active_control_command), active_control_command, this->retries_left_);
        }
#endif
        this->command_sent_ = false;
        this->last_send_ms_ = 0;
        this->send_command_(this->active_command_.type, this->active_command_.data.data(), this->active_command_.len);
        this->retries_left_--;
      } else {
        if (active_control_command != 0) {
          ESP_LOGW(TAG, "Command 0x%04X subcommand 0x%02" PRIX32 " timed out", this->active_command_.type,
                   active_control_command);
        } else {
          ESP_LOGW(TAG, "Command 0x%04X timed out", this->active_command_.type);
        }
        if (this->active_command_.type == TYPE_SET_AREA) {
          this->area_write_pending_ = false;
        }
        this->command_active_ = false;
        this->command_sent_ = false;
        this->active_frame_id_ = 0;
        this->last_send_ms_ = 0;
      }
    }
    return;
  }

  if (this->cmd_count_ == 0)
    return;

  this->active_command_ = this->cmd_queue_[this->cmd_head_];
  this->cmd_head_ = (this->cmd_head_ + 1) % CMD_QUEUE_SIZE;
  this->cmd_count_--;

  this->retries_left_ = CMD_MAX_RETRIES;
  this->command_active_ = true;
  this->command_sent_ = false;
  this->active_frame_id_ = 0;
  this->last_send_ms_ = 0;
  this->send_command_(this->active_command_.type, this->active_command_.data.data(), this->active_command_.len);
}

void LD6002BComponent::send_command_(uint16_t type, const uint8_t *data, uint8_t len) {
  this->send_command_internal_(type, data, len, true);
}

void LD6002BComponent::send_command_untracked_(uint16_t type, const uint8_t *data, uint8_t len) {
  this->send_command_internal_(type, data, len, false);
}

void LD6002BComponent::send_command_internal_(uint16_t type, const uint8_t *data, uint8_t len, bool track) {
  if (this->auto_wake_ && this->wakeup_pin_ != nullptr) {
    if (len > CMD_MAX_DATA_LEN) {
      ESP_LOGW(TAG, "Command data too large: %u", len);
      return;
    }
    std::array<uint8_t, CMD_MAX_DATA_LEN> data_copy{};
    if (len > 0 && data != nullptr) {
      std::memcpy(data_copy.data(), data, len);
    }
    this->wakeup_pin_->digital_write(false);
    this->set_timeout(this->wakeup_pulse_ms_, [this, type, len, data_copy, track]() {
      this->wakeup_pin_->digital_write(true);
      const uint8_t *payload = (len > 0) ? data_copy.data() : nullptr;
      this->write_frame_(type, payload, len, track);
    });
    return;
  }

  this->write_frame_(type, data, len, track);
}

void LD6002BComponent::write_frame_(uint16_t type, const uint8_t *data, uint8_t len, bool track) {
  uint16_t frame_id = this->next_frame_id_++ & 0x7FFF;
  frame_id |= 0x8000;

  uint8_t header_xor = 0;
  auto write_header = [&](uint8_t b) {
    this->write_byte(b);
    header_xor ^= b;
  };

  write_header(TF_SOF);
  write_header((frame_id >> 8) & 0xFF);
  write_header(frame_id & 0xFF);
  write_header((len >> 8) & 0xFF);
  write_header(len & 0xFF);
  write_header((type >> 8) & 0xFF);
  write_header(type & 0xFF);

  this->write_byte(static_cast<uint8_t>(~header_xor));

  if (len > 0 && data != nullptr) {
    uint8_t data_xor = 0;
    for (uint8_t i = 0; i < len; i++) {
      this->write_byte(data[i]);
      data_xor ^= data[i];
    }
    this->write_byte(static_cast<uint8_t>(~data_xor));
  }
  if (track) {
    this->active_frame_id_ = frame_id;
    this->last_send_ms_ = millis();
    this->command_sent_ = true;
  }
}

void LD6002BComponent::send_control_command_(uint32_t command) {
  uint8_t data[4];
  write_u32_le(data, command);
  this->queue_command_(TYPE_CONTROL, data, sizeof(data));
}

void LD6002BComponent::apply_area_config_() {
  if (!this->area_id_set_) {
    ESP_LOGW(TAG, "Area ID not selected; ignoring apply");
    return;
  }
  if (this->area_id_ > 7) {
    ESP_LOGW(TAG, "Invalid area id: %u", this->area_id_);
    return;
  }

  const bool interference = this->area_id_ < AREA_COUNT;
  const uint8_t index = interference ? this->area_id_ : static_cast<uint8_t>(this->area_id_ - AREA_COUNT);
  AreaConfig desired = interference ? this->interference_area_values_[index] : this->detection_area_values_[index];
  if (!std::isnan(this->area_x_min_))
    desired.x_min = this->area_x_min_;
  if (!std::isnan(this->area_x_max_))
    desired.x_max = this->area_x_max_;
  if (!std::isnan(this->area_y_min_))
    desired.y_min = this->area_y_min_;
  if (!std::isnan(this->area_y_max_))
    desired.y_max = this->area_y_max_;
  if (!std::isnan(this->area_z_min_))
    desired.z_min = this->area_z_min_;
  if (!std::isnan(this->area_z_max_))
    desired.z_max = this->area_z_max_;

  if (std::isnan(desired.x_min) || std::isnan(desired.x_max) || std::isnan(desired.y_min) ||
      std::isnan(desired.y_max) || std::isnan(desired.z_min) || std::isnan(desired.z_max)) {
    this->pending_area_apply_ = true;
    this->pending_area_id_ = this->area_id_;
    this->pending_area_updates_ = AreaConfig{};
    this->pending_area_updates_.x_min = this->area_x_min_;
    this->pending_area_updates_.x_max = this->area_x_max_;
    this->pending_area_updates_.y_min = this->area_y_min_;
    this->pending_area_updates_.y_max = this->area_y_max_;
    this->pending_area_updates_.z_min = this->area_z_min_;
    this->pending_area_updates_.z_max = this->area_z_max_;
    ESP_LOGI(TAG, "Area config incomplete; requesting current areas before applying");
    this->send_control_command_(CMD_GET_AREAS);
    return;
  }
  this->queue_area_config_(this->area_id_, desired);
}

void LD6002BComponent::wake_() {
  if (this->wakeup_pin_ == nullptr)
    return;
  this->wakeup_pin_->digital_write(false);
  this->set_timeout(this->wakeup_pulse_ms_, [this]() { this->wakeup_pin_->digital_write(true); });
}

void LD6002BComponent::set_number_value(NumberType type, float value) {
  switch (type) {
    case NumberType::HOLD_DELAY: {
      uint32_t delay = static_cast<uint32_t>(value);
      this->hold_delay_seconds_ = delay;
      uint8_t data[4];
      write_u32_le(data, delay);
      this->queue_command_(TYPE_SET_HOLD_DELAY, data, sizeof(data));
      break;
    }
    case NumberType::Z_MIN:
      this->z_min_ = value;
      if (!std::isnan(this->z_min_) && !std::isnan(this->z_max_)) {
        uint8_t data[8];
        write_f32_le(data, this->z_min_);
        write_f32_le(data + 4, this->z_max_);
        this->queue_command_(TYPE_SET_Z_RANGE, data, sizeof(data));
      }
      break;
    case NumberType::Z_MAX:
      this->z_max_ = value;
      if (!std::isnan(this->z_min_) && !std::isnan(this->z_max_)) {
        uint8_t data[8];
        write_f32_le(data, this->z_min_);
        write_f32_le(data + 4, this->z_max_);
        this->queue_command_(TYPE_SET_Z_RANGE, data, sizeof(data));
      }
      break;
    case NumberType::LOW_POWER_SLEEP: {
      uint32_t sleep_ms = static_cast<uint32_t>(value);
      uint8_t data[4];
      write_u32_le(data, sleep_ms);
      this->queue_command_(TYPE_SET_LOW_POWER_SLEEP, data, sizeof(data));
      break;
    }
    case NumberType::AREA_X_MIN:
      this->area_x_min_ = value;
      break;
    case NumberType::AREA_X_MAX:
      this->area_x_max_ = value;
      break;
    case NumberType::AREA_Y_MIN:
      this->area_y_min_ = value;
      break;
    case NumberType::AREA_Y_MAX:
      this->area_y_max_ = value;
      break;
    case NumberType::AREA_Z_MIN:
      this->area_z_min_ = value;
      break;
    case NumberType::AREA_Z_MAX:
      this->area_z_max_ = value;
      break;
  }
}

void LD6002BComponent::set_select_value(SelectType type, size_t index) {
  switch (type) {
    case SelectType::SENSITIVITY:
      if (index == 0) {
        this->send_control_command_(CMD_SENSITIVITY_LOW);
      } else if (index == 1) {
        this->send_control_command_(CMD_SENSITIVITY_MEDIUM);
      } else if (index == 2) {
        this->send_control_command_(CMD_SENSITIVITY_HIGH);
      }
      break;
    case SelectType::TRIGGER_SPEED:
      if (index == 0) {
        this->send_control_command_(CMD_TRIGGER_SLOW);
      } else if (index == 1) {
        this->send_control_command_(CMD_TRIGGER_MEDIUM);
      } else if (index == 2) {
        this->send_control_command_(CMD_TRIGGER_FAST);
      }
      break;
    case SelectType::INSTALLATION_MODE:
      if (index == 0) {
        this->send_control_command_(CMD_INSTALL_TOP);
      } else if (index == 1) {
        this->send_control_command_(CMD_INSTALL_SIDE);
      }
      if (index <= 1) {
        this->save_installation_pref_(static_cast<uint8_t>(index));
      }
      break;
    case SelectType::AREA_ID:
      this->area_id_ = static_cast<uint8_t>(index);
      this->area_id_set_ = true;
      this->update_area_numbers_for_id_(this->area_id_);
      this->save_area_id_pref_(this->area_id_);
      break;
  }
}

void LD6002BComponent::update_area_numbers_(const AreaConfig &area) {
  this->area_x_min_ = area.x_min;
  this->area_x_max_ = area.x_max;
  this->area_y_min_ = area.y_min;
  this->area_y_max_ = area.y_max;
  this->area_z_min_ = area.z_min;
  this->area_z_max_ = area.z_max;
#ifdef USE_NUMBER
  if (this->area_x_min_number_ != nullptr) {
    this->area_x_min_number_->publish_state(area.x_min);
  }
  if (this->area_x_max_number_ != nullptr) {
    this->area_x_max_number_->publish_state(area.x_max);
  }
  if (this->area_y_min_number_ != nullptr) {
    this->area_y_min_number_->publish_state(area.y_min);
  }
  if (this->area_y_max_number_ != nullptr) {
    this->area_y_max_number_->publish_state(area.y_max);
  }
  if (this->area_z_min_number_ != nullptr) {
    this->area_z_min_number_->publish_state(area.z_min);
  }
  if (this->area_z_max_number_ != nullptr) {
    this->area_z_max_number_->publish_state(area.z_max);
  }
#endif
}

void LD6002BComponent::update_area_numbers_for_id_(uint8_t area_id) {
  if (area_id >= AREA_COUNT * 2)
    return;
  const bool interference = area_id < AREA_COUNT;
  const uint8_t index = interference ? area_id : static_cast<uint8_t>(area_id - AREA_COUNT);
  const AreaConfig &area = interference ? this->interference_area_values_[index] : this->detection_area_values_[index];
  this->update_area_numbers_(area);
}

void LD6002BComponent::queue_area_config_(uint8_t area_id, const AreaConfig &desired) {
  uint8_t data[AREA_CONFIG_LEN];
  write_int32_le(data, static_cast<int32_t>(area_id));
  write_f32_le(data + 4, desired.x_min);
  write_f32_le(data + 8, desired.x_max);
  write_f32_le(data + 12, desired.y_min);
  write_f32_le(data + 16, desired.y_max);
  write_f32_le(data + 20, desired.z_min);
  write_f32_le(data + 24, desired.z_max);

  this->queue_command_(TYPE_SET_AREA, data, sizeof(data));
  this->area_write_pending_ = true;

  const bool interference = area_id < AREA_COUNT;
  const uint8_t index = interference ? area_id : static_cast<uint8_t>(area_id - AREA_COUNT);
  AreaConfig &store = interference ? this->interference_area_values_[index] : this->detection_area_values_[index];
  store = desired;
  this->update_area_numbers_(store);
}

void LD6002BComponent::try_apply_pending_area_() {
  if (!this->pending_area_apply_) {
    return;
  }
  if (this->pending_area_id_ > 7) {
    this->pending_area_apply_ = false;
    return;
  }
  const bool interference = this->pending_area_id_ < AREA_COUNT;
  const uint8_t index =
      interference ? this->pending_area_id_ : static_cast<uint8_t>(this->pending_area_id_ - AREA_COUNT);
  AreaConfig desired = interference ? this->interference_area_values_[index] : this->detection_area_values_[index];

  if (!std::isnan(this->pending_area_updates_.x_min))
    desired.x_min = this->pending_area_updates_.x_min;
  if (!std::isnan(this->pending_area_updates_.x_max))
    desired.x_max = this->pending_area_updates_.x_max;
  if (!std::isnan(this->pending_area_updates_.y_min))
    desired.y_min = this->pending_area_updates_.y_min;
  if (!std::isnan(this->pending_area_updates_.y_max))
    desired.y_max = this->pending_area_updates_.y_max;
  if (!std::isnan(this->pending_area_updates_.z_min))
    desired.z_min = this->pending_area_updates_.z_min;
  if (!std::isnan(this->pending_area_updates_.z_max))
    desired.z_max = this->pending_area_updates_.z_max;

  if (std::isnan(desired.x_min) || std::isnan(desired.x_max) || std::isnan(desired.y_min) ||
      std::isnan(desired.y_max) || std::isnan(desired.z_min) || std::isnan(desired.z_max)) {
    return;
  }

  const uint8_t area_id = this->pending_area_id_;
  this->pending_area_apply_ = false;
  this->queue_area_config_(area_id, desired);
}

void LD6002BComponent::init_area_id_pref_() {
#ifdef USE_SELECT
  if (this->area_id_select_ == nullptr) {
    return;
  }
  this->area_id_pref_ = this->area_id_select_->make_entity_preference<uint8_t>();
  this->area_id_pref_initialized_ = true;

  uint8_t value = 0;
  if (this->area_id_pref_.load(&value) && value < AREA_COUNT * 2) {
    this->area_id_select_->publish_state(value);
    this->area_id_ = value;
    this->area_id_set_ = true;
    this->update_area_numbers_for_id_(value);
  }
#endif
}

void LD6002BComponent::save_area_id_pref_(uint8_t value) {
#ifdef USE_SELECT
  if (!this->area_id_pref_initialized_) {
    return;
  }
  this->area_id_pref_.save(&value);
#endif
}

void LD6002BComponent::init_version_pref_() {
#ifdef USE_TEXT_SENSOR
  if (this->ota_version_text_sensor_ == nullptr) {
    return;
  }
  this->version_pref_ = this->ota_version_text_sensor_->make_entity_preference<VersionPref>();
  this->version_pref_initialized_ = true;

  VersionPref pref{};
  if (this->version_pref_.load(&pref) && pref.value[0] != '\0') {
    pref.value[sizeof(pref.value) - 1] = '\0';
    this->ota_version_text_sensor_->publish_state(pref.value);
  }
#endif
}

void LD6002BComponent::save_version_pref_(const char *value) {
#ifdef USE_TEXT_SENSOR
  if (!this->version_pref_initialized_) {
    return;
  }
  VersionPref pref{};
  std::strncpy(pref.value, value, sizeof(pref.value) - 1);
  pref.value[sizeof(pref.value) - 1] = '\0';
  this->version_pref_.save(&pref);
#endif
}

void LD6002BComponent::set_switch_state(SwitchType type, bool state) {
  switch (type) {
    case SwitchType::LOW_POWER:
      this->low_power_enabled_ = state;
      this->low_power_reported_ = true;
      this->send_control_command_(state ? CMD_LOW_POWER_ON : CMD_LOW_POWER_OFF);
      this->update_work_mode_fallback_();
      break;
    case SwitchType::POINT_CLOUD:
      this->send_control_command_(state ? CMD_POINT_CLOUD_ON : CMD_POINT_CLOUD_OFF);
      break;
    case SwitchType::TARGET_DISPLAY:
      this->send_control_command_(state ? CMD_TARGET_DISPLAY_ON : CMD_TARGET_DISPLAY_OFF);
      break;
  }
}

void LD6002BComponent::press_button(ButtonType type) {
  switch (type) {
    case ButtonType::APPLY_AREA:
      this->apply_area_config_();
      break;
    case ButtonType::AUTO_INTERFERENCE:
      this->send_control_command_(CMD_AUTO_INTERFERENCE);
      break;
    case ButtonType::GET_AREAS:
      this->send_control_command_(CMD_GET_AREAS);
      break;
    case ButtonType::CLEAR_INTERFERENCE:
      this->send_control_command_(CMD_CLEAR_INTERFERENCE);
      break;
    case ButtonType::RESET_DETECTION_AREA:
      this->send_control_command_(CMD_RESET_DETECTION_AREA);
      break;
    case ButtonType::GET_DELAY:
      this->send_control_command_(CMD_GET_DELAY);
      break;
    case ButtonType::GET_SENSITIVITY:
      this->send_control_command_(CMD_GET_SENSITIVITY);
      break;
    case ButtonType::GET_TRIGGER_SPEED:
      this->send_control_command_(CMD_GET_TRIGGER);
      break;
    case ButtonType::GET_Z_RANGE:
      this->send_control_command_(CMD_GET_Z_RANGE);
      break;
    case ButtonType::GET_INSTALLATION:
      this->send_control_command_(CMD_GET_INSTALLATION);
      break;
    case ButtonType::GET_LOW_POWER_MODE:
      this->send_control_command_(CMD_GET_LOW_POWER);
      break;
    case ButtonType::GET_LOW_POWER_SLEEP_TIME:
      this->send_control_command_(CMD_GET_LOW_POWER_SLEEP);
      break;
    case ButtonType::RESET_UNATTENDED:
      this->send_control_command_(CMD_RESET_UNATTENDED);
      break;
    case ButtonType::WAKE:
      this->wake_();
      break;
  }
}

}  // namespace esphome::ld6002b

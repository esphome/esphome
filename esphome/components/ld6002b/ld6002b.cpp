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

static constexpr uint16_t TARGET_DATA_LEN = 20;         // x,y,z,dop_idx,cluster_id
static constexpr uint16_t AREA_DATA_LEN = 24;           // 6 floats
static constexpr uint16_t AREA_CONFIG_LEN = 28;         // int32 + 6 floats
static constexpr uint16_t AREA_PRESENCE_ENTRY_LEN = 4;  // uint32 per detection area

static constexpr uint8_t AREA_ID_DEFAULT = 4;  // detection_area_0 for initial display

static constexpr uint8_t VERSION_QUERY_DATA[] = {0x01, 0x01, 0x00, 0x00};

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

void LD6002BComponent::setup() {
  // Only the point cloud stream needs the larger frame; nothing resizes the buffer after setup.
  bool point_cloud_configured = false;
#ifdef USE_SENSOR
  point_cloud_configured = point_cloud_configured || this->point_count_sensor_ != nullptr;
#endif
#ifdef USE_SWITCH
  point_cloud_configured = point_cloud_configured || this->point_cloud_switch_ != nullptr;
#endif
  this->max_data_len_ = point_cloud_configured ? DEFAULT_MAX_DATA_LEN_POINT_CLOUD : DEFAULT_MAX_DATA_LEN;
  // One allocation for the component lifetime; the parser reuses it for the header and every payload.
  RAMAllocator<uint8_t> allocator;
  this->data_buf_ = allocator.allocate(this->max_data_len_);
  if (this->data_buf_ == nullptr) {
    this->mark_failed(LOG_STR("Failed to allocate frame buffer"));
    return;
  }
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
#ifdef USE_TEXT_SENSOR
    // The work mode fallback reads presence off this stream, so it counts as a
    // consumer of it here.  This only feeds the automatic branch below: with a
    // target_display switch configured that switch still decides, and the
    // fallback weighs no presence at all while the stream is off.
    want_target_stream = want_target_stream || this->work_mode_text_sensor_ != nullptr;
#endif
    bool target_display_controlled = false;
#ifdef USE_SWITCH
    if (this->target_display_switch_ != nullptr) {
      target_display_controlled = true;
      // Nothing reports this switch back, so its restored state is the only state
      // there is.  Restoring through the switch keeps its inversion in the path:
      // the restored value is logical, and turn_on()/turn_off() are what turn it
      // into the raw command, the published state and the stream flag.
      const bool state = this->target_display_switch_->get_initial_state_with_restore_mode().value_or(true);
      if (state) {
        this->target_display_switch_->turn_on();
      } else {
        this->target_display_switch_->turn_off();
      }
    }
#endif
    if (!target_display_controlled) {
      // No switch: the stream follows its consumers.  With none, nothing is sent
      // and the module's own default stands -- but the reports are gated out
      // regardless, because there is nothing configured for them to feed.
      this->target_display_enabled_ = want_target_stream;
      if (want_target_stream) {
        this->send_control_command_(CMD_TARGET_DISPLAY_ON);
      }
    }

    bool point_cloud_controlled = false;
#ifdef USE_SWITCH
    if (this->point_cloud_switch_ != nullptr) {
      point_cloud_controlled = true;
      // The switch owns the stream, so it is also what applies the restored state:
      // driving it rather than the module keeps the entity's inversion in the path.
      const bool state = this->point_cloud_switch_->get_initial_state_with_restore_mode().value_or(false);
      if (state) {
        this->point_cloud_switch_->turn_on();
      } else {
        this->point_cloud_switch_->turn_off();
      }
    }
#endif
    if (!point_cloud_controlled) {
      // No switch: the stream follows the sensor that reads it, which is also what
      // the frame buffer above was sized for.
      bool want_point_cloud = false;
#ifdef USE_SENSOR
      want_point_cloud = this->point_count_sensor_ != nullptr;
#endif
      this->send_control_command_(want_point_cloud ? CMD_POINT_CLOUD_ON : CMD_POINT_CLOUD_OFF);
      this->point_cloud_enabled_ = want_point_cloud;
    }

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
    if (want_low_power) {
      // The module reports this one back, so the query below confirms what it took.
      // Driving the switch applies its inversion; it also marks the restored value
      // as reported, so the work mode fallback runs on that until the query lands.
      const bool state = this->low_power_switch_->get_initial_state_with_restore_mode().value_or(false);
      if (state) {
        this->low_power_switch_->turn_on();
      } else {
        this->low_power_switch_->turn_off();
      }
    }
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

    this->init_area_id_pref_();
    this->init_version_pref_();

#ifdef USE_TEXT_SENSOR
    if (this->ota_version_text_sensor_ != nullptr) {
      this->queue_command_(TYPE_QUERY_VERSION, VERSION_QUERY_DATA, sizeof(VERSION_QUERY_DATA));
    }
#endif
  });
}

void LD6002BComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "HLK-LD6002B:\n"
                "  Auto wake: %s\n"
                "  Max data length: %u",
                this->auto_wake_ ? "true" : "false", static_cast<unsigned>(this->max_data_len_));
  if (this->wakeup_pin_ != nullptr) {
    LOG_PIN("  Wake-up Pin: ", this->wakeup_pin_);
    ESP_LOGCONFIG(TAG, "  Wake Pulse: %" PRIu32 "ms", this->wakeup_pulse_ms_);
  }
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Target Count", this->target_count_sensor_);
  LOG_SENSOR("  ", "Point Count", this->point_count_sensor_);
  for (auto &target : this->targets_) {
    LOG_SENSOR("  ", "Target X", target.x);
    LOG_SENSOR("  ", "Target Y", target.y);
    LOG_SENSOR("  ", "Target Z", target.z);
    LOG_SENSOR("  ", "Target Doppler Index", target.dop_idx);
    LOG_SENSOR("  ", "Target Cluster ID", target.cluster_id);
  }
  for (auto &area : this->interference_areas_) {
    LOG_SENSOR("  ", "Interference Area X Min", area.x_min);
    LOG_SENSOR("  ", "Interference Area X Max", area.x_max);
    LOG_SENSOR("  ", "Interference Area Y Min", area.y_min);
    LOG_SENSOR("  ", "Interference Area Y Max", area.y_max);
    LOG_SENSOR("  ", "Interference Area Z Min", area.z_min);
    LOG_SENSOR("  ", "Interference Area Z Max", area.z_max);
  }
  for (auto &area : this->detection_areas_) {
    LOG_SENSOR("  ", "Detection Area X Min", area.x_min);
    LOG_SENSOR("  ", "Detection Area X Max", area.x_max);
    LOG_SENSOR("  ", "Detection Area Y Min", area.y_min);
    LOG_SENSOR("  ", "Detection Area Y Max", area.y_max);
    LOG_SENSOR("  ", "Detection Area Z Min", area.z_min);
    LOG_SENSOR("  ", "Detection Area Z Max", area.z_max);
  }
#endif
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Presence", this->presence_binary_sensor_);
  for (uint8_t i = 0; i < MAX_TARGETS; i++) {
    LOG_BINARY_SENSOR("  ", "Target Presence", this->target_presence_[i]);
  }
  for (uint8_t i = 0; i < AREA_COUNT; i++) {
    LOG_BINARY_SENSOR("  ", "Detection Area Presence", this->area_presence_[i]);
  }
#endif
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Work Mode", this->work_mode_text_sensor_);
  LOG_TEXT_SENSOR("  ", "OTA Version", this->ota_version_text_sensor_);
#endif
#ifdef USE_NUMBER
  LOG_NUMBER("  ", "Hold Delay", this->hold_delay_number_);
  LOG_NUMBER("  ", "Z Min", this->z_min_number_);
  LOG_NUMBER("  ", "Z Max", this->z_max_number_);
  LOG_NUMBER("  ", "Low Power Sleep", this->low_power_sleep_number_);
  LOG_NUMBER("  ", "Area X Min", this->area_x_min_number_);
  LOG_NUMBER("  ", "Area X Max", this->area_x_max_number_);
  LOG_NUMBER("  ", "Area Y Min", this->area_y_min_number_);
  LOG_NUMBER("  ", "Area Y Max", this->area_y_max_number_);
  LOG_NUMBER("  ", "Area Z Min", this->area_z_min_number_);
  LOG_NUMBER("  ", "Area Z Max", this->area_z_max_number_);
#endif
#ifdef USE_SWITCH
  LOG_SWITCH("  ", "Low Power", this->low_power_switch_);
  LOG_SWITCH("  ", "Point Cloud", this->point_cloud_switch_);
  LOG_SWITCH("  ", "Target Display", this->target_display_switch_);
#endif
#ifdef USE_SELECT
  LOG_SELECT("  ", "Sensitivity", this->sensitivity_select_);
  LOG_SELECT("  ", "Trigger Speed", this->trigger_speed_select_);
  LOG_SELECT("  ", "Installation Mode", this->installation_select_);
  LOG_SELECT("  ", "Area ID", this->area_id_select_);
#endif
}

void LD6002BComponent::loop() {
  while (this->available()) {
    uint8_t byte = this->read();
    this->parse_byte_(byte);
  }
  this->process_command_queue_();
}

void LD6002BComponent::reset_parser_() {
  this->parse_state_ = ParseState::SOF;
  this->header_pos_ = 0;
  this->header_xor_ = 0;
  this->data_len_ = 0;
  this->data_pos_ = 0;
  this->data_xor_ = 0;
  this->discard_remaining_ = 0;
  this->frame_oversize_ = false;
}

void LD6002BComponent::parse_byte_(uint8_t byte) {
  switch (this->parse_state_) {
    case ParseState::DISCARD:
      // discard_remaining_ is unsigned: an unguarded decrement at zero would swallow 4 GB of stream.
      if (this->discard_remaining_ > 0) {
        this->discard_remaining_--;
      }
      if (this->discard_remaining_ == 0) {
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
          this->frame_id_ = read_u16_be(this->data_buf_);
          this->data_len_ = read_u16_be(this->data_buf_ + 2);
          this->frame_type_ = read_u16_be(this->data_buf_ + 4);
          // The length is only trustworthy once the header checksum has been verified, so just
          // remember that the frame is oversized and let the HCK state act on it.
          this->frame_oversize_ = this->data_len_ > this->max_data_len_;
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
      if (this->frame_oversize_) {
        ESP_LOGW(TAG, "Frame too large: %u", this->data_len_);
        // The header is verified, so the length can be trusted: skip the payload and its checksum.
        this->discard_remaining_ = static_cast<uint32_t>(this->data_len_) + 1;
        this->parse_state_ = ParseState::DISCARD;
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
        this->handle_frame_(this->frame_type_, this->data_buf_, this->data_len_);
      } else {
        ESP_LOGV(TAG, "Data checksum mismatch");
      }
      this->reset_parser_();
      return;
    }
  }
}

void LD6002BComponent::handle_frame_(uint16_t type, const uint8_t *data, uint16_t len) {
  this->last_traffic_ms_ = millis();
  if (this->stale_ack_count_ > 0 && millis() - this->stale_ack_ms_ > STALE_ACK_MAX_AGE_MS) {
    this->stale_ack_count_ = 0;
  }
  // ACKs carry no id and arrive in send order: debt from earlier attempts is paid before the active command.
  if (len == 0 && this->stale_ack_count_ > 0 && this->stale_ack_type_ == type) {
    this->stale_ack_count_--;
    ESP_LOGV(TAG, "Ignoring ACK for command 0x%04X from an earlier attempt (module frame 0x%04X)", type,
             this->frame_id_);
    return;
  }
  if (len == 0 && this->command_active_ && this->command_sent_ && type == this->active_command_.type) {
    ESP_LOGV(TAG, "ACK for command 0x%04X (module frame 0x%04X)", type, this->frame_id_);
    const bool refresh_areas = (type == TYPE_SET_AREA) && this->area_write_in_flight_;
    // This settles one expected reply; the rest stay owed and become the debt for the next command.
    this->send_generation_++;
    this->stale_ack_type_ = type;
    this->stale_ack_count_ = this->acks_expected_ > 0 ? static_cast<uint8_t>(this->acks_expected_ - 1) : 0;
    this->stale_ack_ms_ = millis();
    this->command_active_ = false;
    this->command_sent_ = false;
    this->last_send_ms_ = 0;
    this->process_command_queue_();
    if (refresh_areas) {
      this->area_write_in_flight_ = false;
      this->set_timeout(AREA_REFRESH_TIMEOUT, 50, [this]() { this->send_control_command_(CMD_GET_AREAS); });
    }
    return;
  }

#ifdef ESPHOME_LOG_HAS_VERBOSE
  const uint32_t active_control_command =
      (this->command_active_ && this->active_command_.type == TYPE_CONTROL && this->active_command_.len >= 4)
          ? read_u32_le(this->active_command_.data.data())
          : 0;
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
  // The module stops streaming when it acts on the command, not when the command
  // is queued, so trailing frames after an off must not repopulate what
  // set_switch_state just cleared.
  if (!this->target_display_enabled_) {
    return;
  }
  if (len < 4)
    return;

  uint32_t target_num = read_u32_le(data);
  uint16_t available = (len - 4) / TARGET_DATA_LEN;
  // Un-narrowed: a report of e.g. 256 targets must not truncate to 0 and read as "absent".
  const uint32_t reported = std::min<uint32_t>(target_num, available);
  uint8_t count = static_cast<uint8_t>(std::min<uint32_t>(reported, MAX_TARGETS));

  // The module re-sorts its array by cluster id, so slots key on the id to track the person.
  std::array<int32_t, MAX_TARGETS> wire_cluster{};
  std::array<bool, MAX_TARGETS> wire_placed{};
  std::array<bool, MAX_TARGETS> slot_seen{};
  std::array<uint8_t, MAX_TARGETS> slot_wire{};
  for (uint8_t i = 0; i < count; i++) {
    uint16_t cluster_offset = 4 + (i * TARGET_DATA_LEN) + 16;
    wire_cluster[i] = static_cast<int32_t>(read_u32_le(data + cluster_offset));
  }
  for (uint8_t i = 0; i < count; i++) {
    for (uint8_t s = 0; s < MAX_TARGETS; s++) {
      if (this->slot_occupied_[s] && !slot_seen[s] && this->slot_cluster_[s] == wire_cluster[i]) {
        slot_seen[s] = true;
        wire_placed[i] = true;
        slot_wire[s] = i;
        break;
      }
    }
  }
  for (uint8_t s = 0; s < MAX_TARGETS; s++) {
    if (!slot_seen[s]) {
      this->slot_occupied_[s] = false;
    }
  }
  for (uint8_t i = 0; i < count; i++) {
    if (wire_placed[i]) {
      continue;
    }
    for (uint8_t s = 0; s < MAX_TARGETS; s++) {
      if (!this->slot_occupied_[s]) {
        this->slot_occupied_[s] = true;
        this->slot_cluster_[s] = wire_cluster[i];
        slot_wire[s] = i;
        break;
      }
    }
  }

#ifdef USE_SENSOR
  if (this->target_count_sensor_ != nullptr) {
    if (reported != this->last_target_count_) {
      this->target_count_sensor_->publish_state(reported);
      this->last_target_count_ = reported;
    }
  }
#endif

  this->target_presence_any_ = (reported > 0);
#ifdef USE_BINARY_SENSOR
  bool presence = this->target_presence_any_ || this->area_presence_any_;
  if (this->presence_binary_sensor_ != nullptr) {
    this->presence_binary_sensor_->publish_state(presence);
  }
#endif
  this->update_work_mode_fallback_();

  for (uint8_t i = 0; i < MAX_TARGETS; i++) {
    bool has_target = this->slot_occupied_[i];
    if (has_target) {
#ifdef USE_SENSOR
      uint16_t offset = 4 + (slot_wire[i] * TARGET_DATA_LEN);
      float x = read_f32_le(data + offset + 0);
      float y = read_f32_le(data + offset + 4);
      float z = read_f32_le(data + offset + 8);
      int32_t dop_idx = read_int32_le(data + offset + 12);
      int32_t cluster_id = this->slot_cluster_[i];
      TargetSensors &target = this->targets_[i];
      if (target.x != nullptr) {
        target.x->publish_state(x);
      }
      if (target.y != nullptr) {
        target.y->publish_state(y);
      }
      if (target.z != nullptr) {
        target.z->publish_state(z);
      }
      if (target.dop_idx != nullptr) {
        target.dop_idx->publish_state(static_cast<float>(dop_idx));
      }
      if (target.cluster_id != nullptr) {
        if (!this->last_cluster_id_valid_[i] || cluster_id != this->last_cluster_id_[i]) {
          target.cluster_id->publish_state(static_cast<float>(cluster_id));
          this->last_cluster_id_[i] = cluster_id;
          this->last_cluster_id_valid_[i] = true;
        }
      }
#endif
    } else {
#ifdef USE_SENSOR
      this->clear_target_slot_(i);
#endif
    }
#ifdef USE_BINARY_SENSOR
    if (this->target_presence_[i] != nullptr) {
      // publish_state() already skips unchanged states, no manual de-dup needed.
      this->target_presence_[i]->publish_state(has_target);
    }
#endif
#ifdef USE_SENSOR
    this->last_target_presence_[i] = has_target;
#endif
  }
}

void LD6002BComponent::handle_point_cloud_(const uint8_t *data, uint16_t len) {
  // Same window as the target stream: a frame already in flight must not put the
  // count back after the switch cleared it.
  if (!this->point_cloud_enabled_) {
    return;
  }
  if (len < 4)
    return;

#ifdef USE_SENSOR
  uint32_t point_num = read_u32_le(data);
  if (this->point_count_sensor_ != nullptr) {
    if (point_num != this->last_point_count_) {
      this->point_count_sensor_->publish_state(point_num);
      this->last_point_count_ = point_num;
    }
  }
#endif
}

// 0x0A0A carries one uint32 per detection area -- the protocol names the four
// fields detection_state_area0..3 -- so this covers area ids 4..7 only.  The
// interference areas have no presence report: a target inside one is what they
// exist to suppress.
void LD6002BComponent::handle_area_presence_(const uint8_t *data, uint16_t len) {
  const uint16_t needed = AREA_COUNT * AREA_PRESENCE_ENTRY_LEN;
  if (len < needed)
    return;

  this->area_presence_any_ = false;
  for (uint8_t i = 0; i < AREA_COUNT; i++) {
    uint32_t state = read_u32_le(data + (i * AREA_PRESENCE_ENTRY_LEN));
    bool present = state != 0;
    this->area_presence_any_ = this->area_presence_any_ || present;
#ifdef USE_BINARY_SENSOR
    if (this->area_presence_[i] != nullptr) {
      this->area_presence_[i]->publish_state(present);
    }
#endif
  }

#ifdef USE_BINARY_SENSOR
  bool presence = this->target_presence_any_ || this->area_presence_any_;
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
  this->try_apply_pending_area_(interference);
}

void LD6002BComponent::handle_delay_report_(const uint8_t *data, uint16_t len) {
  if (len < 4)
    return;
#ifdef USE_NUMBER
  uint32_t delay = read_u32_le(data);
  this->publish_number_clamped_(this->hold_delay_number_, delay);
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
  this->publish_number_clamped_(this->z_min_number_, z_min);
  this->publish_number_clamped_(this->z_max_number_, z_max);
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
  this->update_work_mode_fallback_();
}

void LD6002BComponent::handle_low_power_sleep_report_(const uint8_t *data, uint16_t len) {
  if (len < 4)
    return;
#ifdef USE_NUMBER
  uint32_t sleep_ms = read_u32_le(data);
  this->publish_number_clamped_(this->low_power_sleep_number_, sleep_ms);
#endif
}

void LD6002BComponent::handle_work_mode_report_(const uint8_t *data, uint16_t len) {
  if (len < 1)
    return;
  // Zero is the unattended half of this transition.  Read outside the text sensor's
  // ifdef because the area sensors do not need one configured to have gone stale.
  const bool low_power = (data[0] == 0);
#ifdef USE_TEXT_SENSOR
  if (this->work_mode_text_sensor_ != nullptr) {
    this->work_mode_reported_ = true;
    this->publish_work_mode_(low_power);
  }
#endif
  // Protocol V1.2 section 2.1.17: this message is sent only on the transition
  // between the unattended low-power mode and normal operation, so a zero is the
  // module stating that nobody is in any area.  Not while a target is still being
  // tracked, though: the reset_unattended command is undocumented on whether it
  // forces this report, and where two statements from the module disagree the live
  // one wins.
  if (low_power && !this->target_presence_any_) {
    this->clear_area_presence_();
  }
}

void LD6002BComponent::update_work_mode_fallback_() {
#ifdef USE_TEXT_SENSOR
  if (this->work_mode_text_sensor_ == nullptr || this->work_mode_reported_) {
    return;
  }
  if (!this->low_power_reported_) {
    return;
  }
  // Target presence is only meaningful while the stream that maintains it runs.
  // Area presence keeps its own report, so it still counts with the target stream
  // off and low power alone decides only when neither half has anything to say.
  const bool presence = (this->target_display_enabled_ && this->target_presence_any_) || this->area_presence_any_;
  this->publish_work_mode_(this->low_power_enabled_ && !presence);
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

#ifdef USE_NUMBER
void LD6002BComponent::publish_number_clamped_(number::Number *number, float value) {
  if (number == nullptr)
    return;
  if (std::isnan(value)) {
    // NAN is this component's "the module has not told us yet".  Publishing it on an
    // entity that has never had a state would report a nan where unknown is the
    // truth; on one that already shows a value it is the only way to say that value
    // no longer describes the selected area.
    if (number->has_state()) {
      number->publish_state(value);
    }
    return;
  }
  const float min_value = number->traits.get_min_value();
  const float max_value = number->traits.get_max_value();
  // Outside the declared range the user cannot write the value back, so publish
  // what they can reach and say what the module actually sent.
  if (value < min_value || value > max_value) {
    ESP_LOGW(TAG, "'%s': module reported %.1f, clamped to %.1f..%.1f", number->get_name().c_str(), value, min_value,
             max_value);
    value = std::clamp(value, min_value, max_value);
  }
  number->publish_state(value);
}
#endif

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

bool LD6002BComponent::queue_command_(uint16_t type, const uint8_t *data, uint8_t len) {
  if (len > CMD_MAX_DATA_LEN) {
    ESP_LOGW(TAG, "Command data too large: %u", len);
    return false;
  }
  if (this->cmd_count_ >= CMD_QUEUE_SIZE) {
    ESP_LOGW(TAG, "Command queue full, dropping command 0x%04X", type);
    return false;
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
  return true;
}

void LD6002BComponent::process_command_queue_() {
  uint32_t now = millis();
  if (this->command_active_) {
    // A sleeping module consumes the opening attempt as its wake-up instead of answering it.
    const uint32_t ack_timeout = this->attempts_sent_ <= 1 ? CMD_FIRST_ACK_TIMEOUT_MS : CMD_ACK_TIMEOUT_MS;
    if (this->command_sent_ && now - this->last_send_ms_ >= ack_timeout) {
      const uint32_t active_control_command =
          (this->active_command_.type == TYPE_CONTROL && this->active_command_.len >= 4)
              ? read_u32_le(this->active_command_.data.data())
              : 0;
      if (this->retries_left_ > 0) {
#ifdef ESPHOME_LOG_HAS_VERBOSE
        if (active_control_command != 0) {
          ESP_LOGV(TAG, "Retrying %s (0x%02" PRIX32 "), %u attempt(s) remaining",
                   control_command_name(active_control_command), active_control_command, this->retries_left_);
        } else {
          // Writes without a control subcommand (hold delay, z-range) had no retry trace at all.
          ESP_LOGV(TAG, "Retrying command 0x%04X, %u attempt(s) remaining", this->active_command_.type,
                   this->retries_left_);
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
          this->area_write_in_flight_ = false;
        }
        // The deferred apply is waiting on the report this command would have
        // brought back, and nothing else re-arms it.  Dropping it here is the
        // difference between one apply lost to a timeout and one that rides in on
        // an unrelated area report later, writing bounds the user has moved on from.
        if (active_control_command == CMD_GET_AREAS && this->deferred_apply_pending_) {
          this->deferred_apply_pending_ = false;
          this->restore_deferred_edits_();
          ESP_LOGW(TAG, "Area read timed out, dropping deferred area apply");
        }
        // A reply may still be in flight for the attempt we just gave up on, so carry one over as
        // debt rather than clearing the ledger, or that late ACK would retire the successor. Only
        // one: reaching this point means nothing was answered at all, so the older attempts are
        // speculative, and carrying them would swallow the successor's own replies.
        const uint16_t owed = (this->stale_ack_type_ == this->active_command_.type ? this->stale_ack_count_ : 0) +
                              (this->acks_expected_ > 0 ? 1 : 0);
        this->stale_ack_type_ = this->active_command_.type;
        this->stale_ack_count_ = static_cast<uint8_t>(std::min<uint16_t>(owed, 255));
        this->stale_ack_ms_ = now;
        this->send_generation_++;
        this->command_active_ = false;
        this->command_sent_ = false;
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

  this->send_generation_++;
  this->retries_left_ = CMD_MAX_RETRIES;
  this->command_active_ = true;
  this->command_sent_ = false;
  this->last_send_ms_ = 0;
  this->attempts_sent_ = 0;
  this->acks_expected_ = 0;
  if (this->stale_ack_type_ != this->active_command_.type) {
    this->stale_ack_count_ = 0;
  }
  this->send_command_(this->active_command_.type, this->active_command_.data.data(), this->active_command_.len);
}

void LD6002BComponent::send_command_(uint16_t type, const uint8_t *data, uint8_t len) {
  this->send_command_internal_(type, data, len, true);
}

void LD6002BComponent::send_command_internal_(uint16_t type, const uint8_t *data, uint8_t len, bool track) {
  if (len > CMD_MAX_DATA_LEN) {
    ESP_LOGW(TAG, "Command data too large: %u", len);
    if (track) {
      // Release the slot: an unwritten command is never acked and never times out.
      this->command_active_ = false;
      this->command_sent_ = false;
      this->last_send_ms_ = 0;
    }
    return;
  }

  // Anonymous timeouts never replace each other; with a pulse already pending the module is waking anyway.
  if (this->auto_wake_ && this->wakeup_pin_ != nullptr && !this->wake_pulse_pending_) {
    // Snapshot the payload: the deferred write must not depend on state a completing command changes.
    if (len > 0 && data != nullptr) {
      std::memcpy(this->wake_scratch_.data(), data, len);
    }
    // A button pulse must not raise the pin in the middle of this one.
    this->cancel_timeout(WAKE_BUTTON_TIMEOUT);
    this->wake_pulse_pending_ = true;
    this->wakeup_pin_->digital_write(false);
    const uint8_t generation = this->send_generation_;
    this->set_timeout(this->wakeup_pulse_ms_, [this, type, len, track, generation]() {
      this->wakeup_pin_->digital_write(true);
      this->wake_pulse_pending_ = false;
      // Anonymous timeouts are never cancelled, so a tracked pulse whose command has since been
      // retired must not transmit: the frame would land after its successor and be booked to it.
      if (track && generation != this->send_generation_) {
        return;
      }
      this->write_frame_(type, (len > 0) ? this->wake_scratch_.data() : nullptr, len, track);
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
  const uint32_t now = millis();
  if (track) {
    // A frame sent to a module that has had time to fall asleep is its wake-up, and goes unanswered.
    if (this->last_traffic_ms_ != 0 && now - this->last_traffic_ms_ < MODULE_AWAKE_MS) {
      this->acks_expected_++;
    }
    this->last_send_ms_ = now;
    this->command_sent_ = true;
    this->attempts_sent_++;
  }
  this->last_traffic_ms_ = now;
}

bool LD6002BComponent::send_control_command_(uint32_t command) {
  uint8_t data[4];
  write_u32_le(data, command);
  return this->queue_command_(TYPE_CONTROL, data, sizeof(data));
}

void LD6002BComponent::send_z_range_() {
  // One frame carries both bounds, so half a range cannot be written.
  if (std::isnan(this->z_min_) || std::isnan(this->z_max_)) {
    ESP_LOGW(TAG, "Z range not written, other bound unknown");
    return;
  }
  // Both bounds are known and crossed; the frame has no way to say that.
  if (this->z_min_ > this->z_max_) {
    ESP_LOGW(TAG, "Z range not written, min above max");
    return;
  }
  uint8_t data[8];
  write_f32_le(data, this->z_min_);
  write_f32_le(data + 4, this->z_max_);
  this->queue_command_(TYPE_SET_Z_RANGE, data, sizeof(data));
}

void LD6002BComponent::apply_area_config_() {
  if (!this->area_id_set_) {
    ESP_LOGW(TAG, "Area ID not selected; ignoring apply");
    return;
  }
  if (this->area_id_ >= AREA_ID_COUNT) {
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
    // Ask first: a read that never reached the queue would leave a deferral waiting
    // on a report nobody requested, with the user's values already retired for it.
    if (!this->send_control_command_(CMD_GET_AREAS)) {
      ESP_LOGW(TAG, "Area read not queued; area config left unapplied");
      return;
    }
    this->deferred_apply_pending_ = true;
    this->pending_area_id_ = this->area_id_;
    // The ledger, not the mirror: the mirror also carries whatever the module last
    // reported for the axes the user never touched, and staging those would hand them
    // back later wearing the user's badge -- a module value the next report is then
    // kept away from.  Staging only what was actually typed is also what makes the
    // replay's overlay right: the untouched axes come from the fresh report.  An
    // empty ledger is a meaning rather than a gap, then: an apply with nothing
    // staged rewrites the area exactly as the report just described it, which is
    // what a direct apply with nothing staged already does.
    this->pending_area_updates_ = this->area_edits_;
    // Staged above, so they are the deferred apply's values now rather than an
    // unsent edit.  Anything typed from here belongs to whatever the user does
    // next, which may well be a different area.
    this->area_edits_ = AreaConfig{};
    ESP_LOGI(TAG, "Area config incomplete; requesting current areas before applying");
    return;
  }
  // Only a write the module will actually see retires them.
  if (this->queue_area_config_(this->area_id_, desired)) {
    this->area_edits_ = AreaConfig{};
  }
}

void LD6002BComponent::wake_() {
  // A command's own pulse raises the pin and writes after it, so ride along instead of
  // claiming the flag: claiming it would send that command down the immediate-write path
  // with the pin still low.
  if (this->wakeup_pin_ == nullptr || this->wake_pulse_pending_)
    return;
  this->wakeup_pin_->digital_write(false);
  this->set_timeout(WAKE_BUTTON_TIMEOUT, this->wakeup_pulse_ms_, [this]() { this->wakeup_pin_->digital_write(true); });
}

void LD6002BComponent::set_number_value(NumberType type, float value) {
  switch (type) {
    case NumberType::HOLD_DELAY: {
      uint32_t delay = static_cast<uint32_t>(value);
      uint8_t data[4];
      write_u32_le(data, delay);
      this->queue_command_(TYPE_SET_HOLD_DELAY, data, sizeof(data));
      break;
    }
    case NumberType::Z_MIN:
      this->z_min_ = value;
      this->send_z_range_();
      break;
    case NumberType::Z_MAX:
      this->z_max_ = value;
      this->send_z_range_();
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
      this->area_edits_.x_min = value;
      break;
    case NumberType::AREA_X_MAX:
      this->area_x_max_ = value;
      this->area_edits_.x_max = value;
      break;
    case NumberType::AREA_Y_MIN:
      this->area_y_min_ = value;
      this->area_edits_.y_min = value;
      break;
    case NumberType::AREA_Y_MAX:
      this->area_y_max_ = value;
      this->area_edits_.y_max = value;
      break;
    case NumberType::AREA_Z_MIN:
      this->area_z_min_ = value;
      this->area_edits_.z_min = value;
      break;
    case NumberType::AREA_Z_MAX:
      this->area_z_max_ = value;
      this->area_edits_.z_max = value;
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
  // A report refreshes every axis the user is not in the middle of changing.  An
  // unapplied edit is the one value here the module cannot know about, so taking
  // the report over it would discard what the user typed with nothing to show for it.
  const AreaConfig &edits = this->area_edits_;
  if (std::isnan(edits.x_min))
    this->area_x_min_ = area.x_min;
  if (std::isnan(edits.x_max))
    this->area_x_max_ = area.x_max;
  if (std::isnan(edits.y_min))
    this->area_y_min_ = area.y_min;
  if (std::isnan(edits.y_max))
    this->area_y_max_ = area.y_max;
  if (std::isnan(edits.z_min))
    this->area_z_min_ = area.z_min;
  if (std::isnan(edits.z_max))
    this->area_z_max_ = area.z_max;
  this->publish_area_numbers_();
}

// The mirror, not the report: an axis a report was kept away from has to keep its
// displayed value too, or the entity and the value the next apply sends disagree.
void LD6002BComponent::publish_area_numbers_() {
#ifdef USE_NUMBER
  this->publish_number_clamped_(this->area_x_min_number_, this->area_x_min_);
  this->publish_number_clamped_(this->area_x_max_number_, this->area_x_max_);
  this->publish_number_clamped_(this->area_y_min_number_, this->area_y_min_);
  this->publish_number_clamped_(this->area_y_max_number_, this->area_y_max_);
  this->publish_number_clamped_(this->area_z_min_number_, this->area_z_min_);
  this->publish_number_clamped_(this->area_z_max_number_, this->area_z_max_);
#endif
}

void LD6002BComponent::update_area_numbers_for_id_(uint8_t area_id) {
  if (area_id >= AREA_ID_COUNT)
    return;
  const bool interference = area_id < AREA_COUNT;
  const uint8_t index = interference ? area_id : static_cast<uint8_t>(area_id - AREA_COUNT);
  const AreaConfig &area = interference ? this->interference_area_values_[index] : this->detection_area_values_[index];
  // The edits belonged to the area being navigated away from.
  this->area_edits_ = AreaConfig{};
  this->update_area_numbers_(area);
}

bool LD6002BComponent::queue_area_config_(uint8_t area_id, const AreaConfig &desired) {
  // One frame carries all three pairs and cannot express a crossed one; the module
  // would keep a box nothing can ever be inside.  Both callers arrive with the six
  // bounds resolved, so this is the last place that can say no -- and the return
  // value is how saying no reaches the caller, which must not then retire the edits
  // the user still has to fix.
  if (desired.x_min > desired.x_max || desired.y_min > desired.y_max || desired.z_min > desired.z_max) {
    ESP_LOGW(TAG, "Area %u not written, min above max", area_id);
    return false;
  }
  uint8_t data[AREA_CONFIG_LEN];
  write_int32_le(data, static_cast<int32_t>(area_id));
  write_f32_le(data + 4, desired.x_min);
  write_f32_le(data + 8, desired.x_max);
  write_f32_le(data + 12, desired.y_min);
  write_f32_le(data + 16, desired.y_max);
  write_f32_le(data + 20, desired.z_min);
  write_f32_le(data + 24, desired.z_max);

  if (!this->queue_command_(TYPE_SET_AREA, data, sizeof(data))) {
    // Nothing is on its way, so the cache must not claim these bounds, the ack
    // refresh must not be armed for an ack that cannot come, and the values stay
    // the user's unsent edit.
    return false;
  }
  this->area_write_in_flight_ = true;

  const bool interference = area_id < AREA_COUNT;
  const uint8_t index = interference ? area_id : static_cast<uint8_t>(area_id - AREA_COUNT);
  AreaConfig &store = interference ? this->interference_area_values_[index] : this->detection_area_values_[index];
  store = desired;
  // The six numbers show one area at a time, and a deferred apply can land here for
  // an area the user has navigated away from.  Same question handle_area_report_
  // asks before it touches them.
  const uint8_t selected_id = this->area_id_set_ ? this->area_id_ : AREA_ID_DEFAULT;
  if (area_id == selected_id) {
    this->update_area_numbers_(store);
  }
  return true;
}

void LD6002BComponent::try_apply_pending_area_(bool reported_interference) {
  if (!this->deferred_apply_pending_) {
    return;
  }
  if (this->pending_area_id_ >= AREA_ID_COUNT) {
    this->deferred_apply_pending_ = false;
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
    // Only the report covering this area's half can still fill it in, and there is
    // exactly one of those per read.  Once it has landed with a bound still unknown,
    // nothing further is coming and waiting means waiting forever.
    if (reported_interference == interference) {
      this->deferred_apply_pending_ = false;
      this->restore_deferred_edits_();
      ESP_LOGW(TAG, "Dropping deferred area apply, area report incomplete");
    }
    return;
  }

  const uint8_t area_id = this->pending_area_id_;
  this->deferred_apply_pending_ = false;
  if (!this->queue_area_config_(area_id, desired)) {
    // Nothing was queued, so this is a drop like the other two: hand the staged
    // values back rather than leaving them with no ledger to protect them.
    this->restore_deferred_edits_();
  }
}

void LD6002BComponent::init_area_id_pref_() {
#ifdef USE_SELECT
  if (this->area_id_select_ == nullptr) {
    return;
  }
  this->area_id_pref_ = this->area_id_select_->make_entity_preference<uint8_t>();
  this->area_id_pref_initialized_ = true;

  uint8_t value = 0;
  if (!this->area_id_pref_.load(&value) || value >= AREA_ID_COUNT) {
    // No stored selection.  The numbers are about to display this area either way,
    // so select it for real: a displayed area that apply_area then refuses to write
    // is the one combination the user cannot make sense of.
    value = AREA_ID_DEFAULT;
  }
  this->area_id_select_->publish_state(value);
  this->area_id_ = value;
  this->area_id_set_ = true;
  this->update_area_numbers_for_id_(value);
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

#ifdef USE_SENSOR
void LD6002BComponent::clear_target_slot_(uint8_t index) {
  if (!this->last_target_presence_[index]) {
    return;
  }
  TargetSensors &target = this->targets_[index];
  if (target.x != nullptr) {
    target.x->publish_state(NAN);
  }
  if (target.y != nullptr) {
    target.y->publish_state(NAN);
  }
  if (target.z != nullptr) {
    target.z->publish_state(NAN);
  }
  if (target.dop_idx != nullptr) {
    target.dop_idx->publish_state(NAN);
  }
  if (target.cluster_id != nullptr) {
    target.cluster_id->publish_state(NAN);
  }
  // The slot is free: the next person's id is new even when it repeats this one.
  this->last_cluster_id_valid_[index] = false;
}
#endif

void LD6002BComponent::restore_deferred_edits_() {
  // The staged values become an unsent edit again, but only for the user who is
  // still looking at the area they were staged for; anyone else's ledger belongs to
  // the area they are on now.
  const uint8_t selected_id = this->area_id_set_ ? this->area_id_ : AREA_ID_DEFAULT;
  if (this->pending_area_id_ != selected_id) {
    return;
  }
  // Axis by axis rather than a whole-struct assignment: the user can have edited
  // another bound while the deferral was in flight, and that edit is newer than
  // anything the deferral staged.  Assigning over the ledger would drop it back to
  // NaN and let the next report take the value away.  A live edit wins; only an axis
  // with nothing in the ledger takes its staged value back.
  //
  // The mirror moves with the ledger, because on the report path handle_area_report_
  // ran update_area_numbers_ before the replay, with the ledger still empty -- so the
  // mirror already holds the module's bounds and both the entities and the next apply
  // would build on them.  On the timeout path no report arrived, the mirror still
  // holds the staged values, and this is an identity.
  const AreaConfig &staged = this->pending_area_updates_;
  if (std::isnan(this->area_edits_.x_min) && !std::isnan(staged.x_min)) {
    this->area_edits_.x_min = staged.x_min;
    this->area_x_min_ = staged.x_min;
  }
  if (std::isnan(this->area_edits_.x_max) && !std::isnan(staged.x_max)) {
    this->area_edits_.x_max = staged.x_max;
    this->area_x_max_ = staged.x_max;
  }
  if (std::isnan(this->area_edits_.y_min) && !std::isnan(staged.y_min)) {
    this->area_edits_.y_min = staged.y_min;
    this->area_y_min_ = staged.y_min;
  }
  if (std::isnan(this->area_edits_.y_max) && !std::isnan(staged.y_max)) {
    this->area_edits_.y_max = staged.y_max;
    this->area_y_max_ = staged.y_max;
  }
  if (std::isnan(this->area_edits_.z_min) && !std::isnan(staged.z_min)) {
    this->area_edits_.z_min = staged.z_min;
    this->area_z_min_ = staged.z_min;
  }
  if (std::isnan(this->area_edits_.z_max) && !std::isnan(staged.z_max)) {
    this->area_edits_.z_max = staged.z_max;
    this->area_z_max_ = staged.z_max;
  }
  this->publish_area_numbers_();
}

void LD6002BComponent::clear_area_presence_() {
  if (!this->area_presence_any_) {
    return;
  }
  // Nothing else corrects this: 0x0A0A carries no period the protocol states and no
  // command stops it, so the module going unattended is the only moment the
  // component can know a stored "occupied" has stopped being true.
  this->area_presence_any_ = false;
#ifdef USE_BINARY_SENSOR
  for (uint8_t i = 0; i < AREA_COUNT; i++) {
    if (this->area_presence_[i] != nullptr) {
      this->area_presence_[i]->publish_state(false);
    }
  }
  const bool presence = this->target_presence_any_ || this->area_presence_any_;
  if (this->presence_binary_sensor_ != nullptr) {
    this->presence_binary_sensor_->publish_state(presence);
  }
#endif
}

void LD6002BComponent::clear_target_state_() {
  // Nothing corrects any of this until the stream comes back.  The slot table goes
  // with it: slots key on cluster ids, which only track a person while reports are
  // arriving, and the room can empty and refill across the gap -- so the next
  // report starts from an empty table and fills slots in wire order, rather than
  // handing one back to whoever last held that id.
  for (uint8_t i = 0; i < MAX_TARGETS; i++) {
#ifdef USE_SENSOR
    this->clear_target_slot_(i);
    this->last_target_presence_[i] = false;
#endif
    if (this->slot_occupied_[i]) {
      this->slot_occupied_[i] = false;
#ifdef USE_BINARY_SENSOR
      if (this->target_presence_[i] != nullptr) {
        this->target_presence_[i]->publish_state(false);
      }
#endif
    }
  }
#ifdef USE_SENSOR
  if (this->last_target_count_ != 0xFFFFFFFF) {
    if (this->target_count_sensor_ != nullptr) {
      this->target_count_sensor_->publish_state(NAN);
    }
    this->last_target_count_ = 0xFFFFFFFF;
  }
#endif
  if (this->target_presence_any_) {
    this->target_presence_any_ = false;
#ifdef USE_BINARY_SENSOR
    bool presence = this->target_presence_any_ || this->area_presence_any_;
    if (this->presence_binary_sensor_ != nullptr) {
      this->presence_binary_sensor_->publish_state(presence);
    }
#endif
    this->update_work_mode_fallback_();
  }
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
      this->point_cloud_enabled_ = state;
      this->send_control_command_(state ? CMD_POINT_CLOUD_ON : CMD_POINT_CLOUD_OFF);
#ifdef USE_SENSOR
      // The count only moves while the stream runs, so the last one would stand as
      // a live reading.  The dedup sentinel is cleared with it: the same count is
      // new again when the stream comes back.
      if (!state && this->point_count_sensor_ != nullptr && this->last_point_count_ != 0xFFFFFFFF) {
        this->point_count_sensor_->publish_state(NAN);
        this->last_point_count_ = 0xFFFFFFFF;
      }
#endif
      break;
    case SwitchType::TARGET_DISPLAY:
      this->target_display_enabled_ = state;
      this->send_control_command_(state ? CMD_TARGET_DISPLAY_ON : CMD_TARGET_DISPLAY_OFF);
      if (!state) {
        // Every target entity is fed by the reports this just stopped.
        this->clear_target_state_();
      }
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
      // The module recomputes the interference areas without reporting them.
      this->send_control_command_(CMD_GET_AREAS);
      break;
    case ButtonType::GET_AREAS:
      this->send_control_command_(CMD_GET_AREAS);
      break;
    case ButtonType::CLEAR_INTERFERENCE:
      this->send_control_command_(CMD_CLEAR_INTERFERENCE);
      // The module rewrites the areas but does not report them, so ask for the new geometry the
      // way the apply_area ack path does; the queue keeps it behind the command above.
      this->send_control_command_(CMD_GET_AREAS);
      break;
    case ButtonType::RESET_DETECTION_AREA:
      this->send_control_command_(CMD_RESET_DETECTION_AREA);
      this->send_control_command_(CMD_GET_AREAS);
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

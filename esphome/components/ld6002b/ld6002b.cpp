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

static constexpr uint16_t TYPE_REPORT_TARGET = 0x0A04;
static constexpr uint16_t TYPE_REPORT_AREA_PRESENCE = 0x0A0A;

// Control command values for TYPE_CONTROL
static constexpr uint32_t CMD_POINT_CLOUD_ON = 0x06;
static constexpr uint32_t CMD_POINT_CLOUD_OFF = 0x07;
static constexpr uint32_t CMD_TARGET_DISPLAY_ON = 0x08;
static constexpr uint32_t CMD_TARGET_DISPLAY_OFF = 0x09;

static constexpr uint16_t TARGET_DATA_LEN = 20;  // x,y,z,dop_idx,cluster_id

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
    case CMD_POINT_CLOUD_ON:
      return "point_cloud_on";
    case CMD_POINT_CLOUD_OFF:
      return "point_cloud_off";
    case CMD_TARGET_DISPLAY_ON:
      return "target_display_on";
    case CMD_TARGET_DISPLAY_OFF:
      return "target_display_off";
    default:
      return "unknown";
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
    }

    bool want_point_cloud_stream = false;
    this->send_control_command_(want_point_cloud_stream ? CMD_POINT_CLOUD_ON : CMD_POINT_CLOUD_OFF);
  });
}

void LD6002BComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HLK-LD6002B:");
  ESP_LOGCONFIG(TAG, "  Auto wake: %s", this->auto_wake_ ? "true" : "false");
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Presence", this->presence_binary_sensor_);
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
    this->command_active_ = false;
    this->command_sent_ = false;
    this->active_frame_id_ = 0;
    this->last_send_ms_ = 0;
    this->process_command_queue_();
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
#endif

  switch (type) {
    case TYPE_REPORT_TARGET:
      this->handle_target_report_(data, len);
      break;
    case TYPE_REPORT_AREA_PRESENCE:
      this->handle_area_presence_(data, len);
      break;
    default:
      break;
  }
}

void LD6002BComponent::handle_target_report_(const uint8_t *data, uint16_t len) {
  if (len < 4)
    return;

  uint32_t target_num = read_u32_le(data);
  uint16_t available = (len - 4) / TARGET_DATA_LEN;
  uint8_t count = static_cast<uint8_t>(std::min<uint32_t>(target_num, available));

  this->target_presence_any_ = (count > 0);
  bool presence = this->target_presence_any_ || this->area_presence_any_;
#ifdef USE_BINARY_SENSOR
  if (this->presence_binary_sensor_ != nullptr) {
    this->presence_binary_sensor_->publish_state(presence);
  }
#endif

  for (uint8_t i = 0; i < MAX_TARGETS; i++) {
    bool has_target = i < count;
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

}  // namespace esphome::ld6002b

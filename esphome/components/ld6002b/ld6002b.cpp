#include "ld6002b.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cinttypes>
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

void LD6002BComponent::write_u32_le(uint8_t *data, uint32_t value) {
  data[0] = value & 0xFF;
  data[1] = (value >> 8) & 0xFF;
  data[2] = (value >> 16) & 0xFF;
  data[3] = (value >> 24) & 0xFF;
}

void LD6002BComponent::setup() {
  size_t desired_data_len = this->max_data_len_;
  if (!this->max_data_len_overridden_) {
    desired_data_len = DEFAULT_MAX_DATA_LEN;
  }
  this->max_data_len_ = desired_data_len;
  // One allocation for the component lifetime: the parser reuses this buffer
  // for the six byte header and for every payload up to max_data_len_.
  RAMAllocator<uint8_t> allocator;
  this->data_buf_ = allocator.allocate(std::max<size_t>(desired_data_len, 6));
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

    // Point-cloud streaming is introduced in a later part; make sure it is off.
    this->send_control_command_(CMD_POINT_CLOUD_OFF);
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
    ESP_LOGCONFIG(TAG, "  Wake Pulse: %ums", this->wakeup_pulse_ms_);
  }
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Presence", this->presence_binary_sensor_);
  for (uint8_t i = 0; i < MAX_TARGETS; i++) {
    LOG_BINARY_SENSOR("  ", "Target Presence", this->target_presence_[i]);
  }
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
      // Only entered from HCK, with a verified and non-zero number of bytes to skip. The zero
      // guard stays because discard_remaining_ is unsigned: should a later change ever reach
      // this state with nothing left to skip, an unconditional decrement would wrap to
      // 4294967295 and silently swallow the next four gigabytes of stream instead of
      // resynchronising on the following frame.
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
  // The module acknowledges a command with an empty frame of the same type. It does not echo our
  // frame id: that field is a per-peer send counter (MSB 1 for us, 0 for the module) which the
  // protocol only reuses in a reply for message types supporting a bidirectional exchange, and
  // every type sent here is documented as unidirectional. ACKs of one type can therefore only be
  // told apart by transmission order, so the duplicates a retried command leaves behind are
  // swallowed instead of completing whatever command runs next.
  //
  // That swallowing only applies while no matching command is waiting. An ACK that arrives for the
  // command still on the wire always completes it, however late: waking a sleeping module pushes
  // its reply past the retry period, and treating such a reply as a leftover duplicate would starve
  // the command through every attempt and report a timeout for a write the module did perform.
  if (len == 0 && this->stale_ack_count_ > 0 && this->stale_ack_type_ == type &&
      (!this->command_active_ || type != this->active_command_.type)) {
    this->stale_ack_count_--;
    ESP_LOGV(TAG, "Ignoring ACK for command 0x%04X from an earlier attempt (module frame 0x%04X)", type,
             this->frame_id_);
    return;
  }
  if (len == 0 && this->command_active_ && type == this->active_command_.type) {
    ESP_LOGV(TAG, "ACK for command 0x%04X (module frame 0x%04X)", type, this->frame_id_);
    // Every frame transmitted for this command is answered, and this ACK settles exactly one of
    // them, so the remaining attempts are still owed replies. Those become the guard debt for the
    // window after this command finishes.
    this->stale_ack_type_ = type;
    this->stale_ack_count_ = this->attempts_sent_ > 0 ? static_cast<uint8_t>(this->attempts_sent_ - 1) : 0;
    this->command_active_ = false;
    this->command_sent_ = false;
    this->last_send_ms_ = 0;
    this->process_command_queue_();
    return;
  }

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
  // Keep the un-narrowed value for the presence test: a report of e.g. 256
  // targets must not truncate to 0 and read as "absent".
  const uint32_t reported = std::min<uint32_t>(target_num, available);
  uint8_t count = static_cast<uint8_t>(std::min<uint32_t>(reported, MAX_TARGETS));

  this->target_presence_any_ = (reported > 0);
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
      // publish_state() already skips unchanged states, no manual de-dup needed.
      this->target_presence_[i]->publish_state(has_target);
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
    // The opening attempt may be the frame that wakes a sleeping module, which consumes it rather
    // than answering it; the retry that follows is acked 100-175ms later. The wider budget for that
    // first attempt keeps the wake from cascading into further retries, while later attempts keep
    // the normal period.
    const uint32_t ack_timeout = this->attempts_sent_ <= 1 ? CMD_FIRST_ACK_TIMEOUT_MS : CMD_ACK_TIMEOUT_MS;
    if (this->command_sent_ && now - this->last_send_ms_ >= ack_timeout) {
      const uint32_t active_control_command =
          this->active_command_.type == TYPE_CONTROL
              ? read_control_command_value(this->active_command_.data.data(), this->active_command_.len)
              : 0;
      if (this->retries_left_ > 0) {
#ifdef ESPHOME_LOG_HAS_VERBOSE
        if (active_control_command != 0) {
          ESP_LOGV(TAG, "Retrying %s (0x%02" PRIX32 "), %u attempt(s) remaining",
                   control_command_name(active_control_command), active_control_command, this->retries_left_);
        } else {
          // Writes such as the hold delay and z-range carry no control subcommand, so without this
          // branch their retries were invisible and only the eventual timeout showed up.
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
        // No attempt was answered, so the module owes no ACK for this command any more.
        this->stale_ack_count_ = 0;
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

  this->retries_left_ = CMD_MAX_RETRIES;
  this->command_active_ = true;
  this->command_sent_ = false;
  this->last_send_ms_ = 0;
  this->attempts_sent_ = 0;
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
      // Release the slot: this command is never written, so it would neither be
      // acked nor time out and the queue would stall behind it.
      this->command_active_ = false;
      this->command_sent_ = false;
      this->last_send_ms_ = 0;
    }
    return;
  }

  if (this->auto_wake_ && this->wakeup_pin_ != nullptr) {
    // Capture no payload: tracked commands are always sent from
    // active_command_.data, which stays valid until the ack or the timeout,
    // and untracked ones are staged in wake_scratch_ instead.
    if (!track && len > 0 && data != nullptr) {
      std::memcpy(this->wake_scratch_.data(), data, len);
    }
    this->wakeup_pin_->digital_write(false);
    this->set_timeout(this->wakeup_pulse_ms_, [this, type, len, track]() {
      this->wakeup_pin_->digital_write(true);
      const uint8_t *payload = track ? this->active_command_.data.data() : this->wake_scratch_.data();
      this->write_frame_(type, (len > 0) ? payload : nullptr, len, track);
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
    this->last_send_ms_ = millis();
    this->command_sent_ = true;
    this->attempts_sent_++;
  }
}

void LD6002BComponent::send_control_command_(uint32_t command) {
  uint8_t data[4];
  write_u32_le(data, command);
  this->queue_command_(TYPE_CONTROL, data, sizeof(data));
}

}  // namespace esphome::ld6002b

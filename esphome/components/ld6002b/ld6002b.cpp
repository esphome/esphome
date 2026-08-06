#include "ld6002b.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstring>

namespace esphome::ld6002b {

static const char *const TAG = "ld6002b";

static constexpr uint8_t TF_SOF = 0x01;
static constexpr uint32_t SETUP_DELAY_MS = 100;

// Command/message types
static constexpr uint16_t TYPE_CONTROL = 0x0201;

static constexpr uint16_t TYPE_REPORT_TARGET = 0x0A04;

// Control command values for TYPE_CONTROL
static constexpr uint32_t CMD_POINT_CLOUD_ON = 0x06;
static constexpr uint32_t CMD_POINT_CLOUD_OFF = 0x07;
static constexpr uint32_t CMD_TARGET_DISPLAY_ON = 0x08;
static constexpr uint32_t CMD_TARGET_DISPLAY_OFF = 0x09;

static constexpr uint16_t TARGET_DATA_LEN = 20;  // x,y,z,dop_idx,cluster_id

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

void LD6002BComponent::setup() {
  // One allocation for the component lifetime; the parser reuses it for the header and every payload.
  RAMAllocator<uint8_t> allocator;
  this->data_buf_ = allocator.allocate(DEFAULT_MAX_DATA_LEN);
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
    if (want_target_stream) {
      this->send_control_command_(CMD_TARGET_DISPLAY_ON);
    }

    this->send_control_command_(CMD_POINT_CLOUD_OFF);
  });
}

void LD6002BComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "HLK-LD6002B:\n"
                "  Auto wake: %s",
                this->auto_wake_ ? "true" : "false");
  if (this->wakeup_pin_ != nullptr) {
    LOG_PIN("  Wake-up Pin: ", this->wakeup_pin_);
    ESP_LOGCONFIG(TAG, "  Wake Pulse: %ums", this->wakeup_pulse_ms_);
  }
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Target Count", this->target_count_sensor_);
  for (auto &target : this->targets_) {
    LOG_SENSOR("  ", "Target X", target.x);
    LOG_SENSOR("  ", "Target Y", target.y);
    LOG_SENSOR("  ", "Target Z", target.z);
    LOG_SENSOR("  ", "Target Doppler Index", target.dop_idx);
    LOG_SENSOR("  ", "Target Cluster ID", target.cluster_id);
  }
#endif
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
          this->frame_oversize_ = this->data_len_ > DEFAULT_MAX_DATA_LEN;
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
    // This settles one expected reply; the rest stay owed and become the debt for the next command.
    this->send_generation_++;
    this->stale_ack_type_ = type;
    this->stale_ack_count_ = this->acks_expected_ > 0 ? static_cast<uint8_t>(this->acks_expected_ - 1) : 0;
    this->stale_ack_ms_ = millis();
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
    default:
      break;
  }
}

void LD6002BComponent::handle_target_report_(const uint8_t *data, uint16_t len) {
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
  if (this->presence_binary_sensor_ != nullptr) {
    this->presence_binary_sensor_->publish_state(this->target_presence_any_);
  }
#endif

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
      TargetSensors &target = this->targets_[i];
      if (this->last_target_presence_[i]) {
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
        this->last_cluster_id_valid_[i] = false;
      }
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

void LD6002BComponent::send_control_command_(uint32_t command) {
  uint8_t data[4];
  write_u32_le(data, command);
  this->queue_command_(TYPE_CONTROL, data, sizeof(data));
}

}  // namespace esphome::ld6002b

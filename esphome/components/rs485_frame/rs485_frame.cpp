#include "rs485_frame.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

#include <algorithm>
#include <cinttypes>

namespace esphome::rs485_frame {

ESPHOME_LOG_TAG(TAG, "rs485_frame");

PROGMEM_STRING_TABLE(CrcTypeStrings, "none", "sum8", "sum16_big_endian", "sum16_little_endian", "xor8", "crc16_modbus");
PROGMEM_STRING_TABLE(CrcVariantStrings, "header_inclusive", "payload_only");
PROGMEM_STRING_TABLE(GateModeStrings, "frame_trigger", "idle_gap", "fixed_delay");
PROGMEM_STRING_TABLE(QueuePolicyStrings, "replace_latest", "fifo");

void RS485FrameHub::setup() {
  const size_t tx_slot_capacity = this->max_frame_length_ * 2 + FRAME_OVERHEAD_BYTES;
  this->raw_frame_ = std::make_unique<uint8_t[]>(this->max_frame_length_);
  this->rx_payload_.reserve(this->max_frame_length_);
  this->tx_payload_buf_.reserve(this->max_frame_length_);
  this->tx_frame_buf_.reserve(tx_slot_capacity);
  this->pending_tx_frame_.reserve(tx_slot_capacity);
  this->tx_queue_.resize(this->max_queue_size_);
  for (auto &slot : this->tx_queue_)
    slot.reserve(tx_slot_capacity);
  this->hex_log_buf_size_ = tx_slot_capacity * 2 + 1;
  this->hex_log_buf_ = std::make_unique<char[]>(this->hex_log_buf_size_);
}

void RS485FrameHub::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  this->read_available([this, now](uint8_t byte) { this->handle_rx_byte_(byte, now); });

  // A partial frame left by a cable pull or noise burst would otherwise stall reception.
  if (this->in_frame_ && this->in_frame_timeout_ms_ > 0 && now - this->last_rx_time_ >= this->in_frame_timeout_ms_) {
    ESP_LOGW(TAG, "Intra-frame timeout, resetting receive state");
    this->reset_rx_();
    this->previous_byte_ = 0;
  }

  this->maybe_tx_(now);
  if (this->tx_start_pending_ && now - this->tx_start_at_ < 0x80000000UL) {
    this->tx_start_pending_ = false;
    this->transmit_(this->pending_tx_frame_, now, this->pending_is_idle_);
  }
}

void RS485FrameHub::dump_config() {
  char gate_hex[format_hex_size(MAX_FRAME_TYPE_LEN)];
  format_hex_to(gate_hex, this->tx_gate_frame_type_.data(), this->tx_gate_frame_type_.size());
  ESP_LOGCONFIG(TAG,
                "RS485 Frame:\n"
                "  Framing: DLE=0x%02x STX=0x%02x ETX=0x%02x, escape=0x%02x\n"
                "  CRC type: %s, TX CRC variant: %s, accept header CRC: %s, accept payload CRC: %s\n"
                "  TX gate: %s, gate frame: %s, gate delay: %" PRIu32 "ms\n"
                "  TX idle gap: %" PRIu32 "ms, TX interval: %" PRIu32 "ms\n"
                "  Queue policy: %s, queue size: %" PRIu32 "\n"
                "  Max frame length: %" PRIu32 ", frame timeout: %" PRIu32 "ms\n"
                "  Sniffer only: %s, dump frames: %s, idle frame: %zu bytes",
                this->dle_, this->stx_, this->etx_, this->escape_marker_,
                LOG_STR_ARG(CrcTypeStrings::get_log_str(this->crc_type_, 0)),
                LOG_STR_ARG(CrcVariantStrings::get_log_str(this->tx_crc_variant_, 0)), YESNO(this->accept_header_crc_),
                YESNO(this->accept_payload_crc_), LOG_STR_ARG(GateModeStrings::get_log_str(this->tx_gate_mode_, 0)),
                gate_hex, this->tx_gate_delay_, this->tx_idle_gap_, this->tx_fixed_interval_,
                LOG_STR_ARG(QueuePolicyStrings::get_log_str(this->queue_policy_, 0)), this->max_queue_size_,
                this->max_frame_length_, this->in_frame_timeout_ms_, YESNO(this->sniffer_only_),
                YESNO(this->dump_frames_), this->idle_frame_len_);
}

void RS485FrameHub::set_framing(uint8_t dle, uint8_t stx, uint8_t etx, uint8_t escape_marker) {
  this->dle_ = dle;
  this->stx_ = stx;
  this->etx_ = etx;
  this->escape_marker_ = escape_marker;
}

bool RS485FrameHub::check_tx_(size_t len) {
  if (this->sniffer_only_) {
    ESP_LOGW(TAG, "Ignoring frame because sniffer_only is enabled");
  } else if (len > this->max_frame_length_) {
    ESP_LOGW(TAG, "Frame payload (%zu) exceeds max_frame_length (%" PRIu32 "); dropping", len, this->max_frame_length_);
  } else {
    return true;
  }
  this->command_drops_++;
  return false;
}

bool RS485FrameHub::queue_frame(const uint8_t *data, size_t len) {
  if (!this->check_tx_(len))
    return false;
  this->tx_payload_buf_.resize(len);
  progmem_memcpy(this->tx_payload_buf_.data(), data, len);
  this->build_frame_(this->tx_payload_buf_, this->tx_frame_buf_);
  return this->enqueue_frame_();
}

bool RS485FrameHub::queue_raw_frame(const std::vector<uint8_t> &frame_type, const std::vector<uint8_t> &payload) {
  if (!this->check_tx_(frame_type.size() + payload.size()))
    return false;
  this->tx_payload_buf_.assign(frame_type.begin(), frame_type.end());
  this->tx_payload_buf_.insert(this->tx_payload_buf_.end(), payload.begin(), payload.end());
  this->build_frame_(this->tx_payload_buf_, this->tx_frame_buf_);
  return this->enqueue_frame_();
}

bool RS485FrameHub::enqueue_frame_() {
  if (this->queue_policy_ == QUEUE_REPLACE_LATEST) {
    if (this->tx_queue_count_ > 0 || this->tx_start_pending_) {
      ESP_LOGW(TAG, "Replacing pending frame before it was transmitted");
      this->command_drops_++;
    }
    if (this->tx_start_pending_) {
      std::swap(this->pending_tx_frame_, this->tx_frame_buf_);
      this->pending_is_idle_ = false;
      return true;
    }
    this->tx_queue_head_ = 0;
    this->tx_queue_tail_ = 0;
    this->tx_queue_count_ = 0;
  } else if (this->tx_queue_count_ + (this->tx_start_pending_ ? 1 : 0) >= this->max_queue_size_) {
    ESP_LOGW(TAG, "Frame queue full; dropping frame");
    this->command_drops_++;
    return false;
  }
  std::swap(this->tx_queue_[this->tx_queue_tail_], this->tx_frame_buf_);
  this->tx_queue_tail_ = (this->tx_queue_tail_ + 1) % this->max_queue_size_;
  this->tx_queue_count_++;
  return true;
}

void RS485FrameHub::reset_rx_() {
  this->discarded_frames_++;
  this->in_frame_ = false;
  this->after_dle_ = false;
  this->raw_len_ = 0;
}

void RS485FrameHub::handle_rx_byte_(uint8_t byte, uint32_t now) {
  this->last_rx_time_ = now;
  this->last_activity_time_ = now;
  this->has_activity_ = true;
  const uint8_t previous = this->previous_byte_;
  this->previous_byte_ = byte;
  if (!this->in_frame_) {
    if (previous == this->dle_ && byte == this->stx_) {
      this->in_frame_ = true;
      this->after_dle_ = false;
      this->raw_frame_[0] = this->dle_;
      this->raw_frame_[1] = this->stx_;
      this->raw_len_ = 2;
    }
    return;
  }
  if (this->raw_len_ >= this->max_frame_length_) {
    ESP_LOGW(TAG, "Frame exceeded max_frame_length");
    this->reset_rx_();
    return;
  }
  this->raw_frame_[this->raw_len_++] = byte;
  // Each in-frame DLE is resolved by the next byte: ETX ends the frame, the escape marker
  // makes a literal DLE, anything else fails validate_frame_().
  if (this->after_dle_) {
    this->after_dle_ = false;
    if (byte == this->etx_) {
      this->process_raw_frame_(now);
      this->in_frame_ = false;
      this->raw_len_ = 0;
    }
  } else if (byte == this->dle_) {
    this->after_dle_ = true;
  }
}

void RS485FrameHub::process_raw_frame_(uint32_t now) {
  if (!this->validate_frame_()) {
    this->crc_failures_++;
    return;
  }
  this->frames_received_++;
  format_hex_to(this->last_frame_type_, this->rx_payload_.data(), std::min(this->rx_payload_.size(), size_t(2)));
  if (this->dump_frames_)
    this->log_frame_("RX", this->raw_frame_.get(), this->raw_len_);

  const auto &gate = this->tx_gate_frame_type_;
  if (!gate.empty() && this->rx_payload_.size() >= gate.size() &&
      std::equal(gate.begin(), gate.end(), this->rx_payload_.begin())) {
    if (this->last_ka_seen_)
      this->last_keepalive_ms_ = now - this->last_ka_time_;
    this->last_ka_seen_ = true;
    this->last_ka_time_ = now;
    if (this->tx_gate_mode_ == TX_GATE_FRAME_TRIGGER)
      this->send_next_(now);
  }
  this->frame_callback_.call(this->rx_payload_);
}

bool RS485FrameHub::validate_frame_() {
  const uint8_t *frame = this->raw_frame_.get();
  const size_t len = this->raw_len_;
  // 6 = DLE STX + 2 frame_type bytes + DLE ETX.
  if (len < 6 || frame[0] != this->dle_ || frame[1] != this->stx_ || frame[len - 2] != this->dle_ ||
      frame[len - 1] != this->etx_)
    return false;

  auto &payload = this->rx_payload_;
  payload.clear();
  for (size_t i = 2; i + 2 < len; i++) {
    uint8_t b = frame[i];
    if (b == this->dle_) {
      if (i + 1 < len - 2 && frame[i + 1] == this->escape_marker_) {
        payload.push_back(this->dle_);
        i++;
        continue;
      }
      return false;
    }
    payload.push_back(b);
  }
  const size_t crc_len = this->crc_length_();
  if (payload.size() < 2 + crc_len)
    return false;
  if (crc_len == 0)
    return true;

  const uint8_t *crc_bytes = payload.data() + payload.size() - crc_len;
  uint16_t received_crc;
  if (crc_len == 1) {
    received_crc = crc_bytes[0];
  } else if (this->crc_little_endian_()) {
    received_crc = encode_uint16(crc_bytes[1], crc_bytes[0]);
  } else {
    received_crc = encode_uint16(crc_bytes[0], crc_bytes[1]);
  }
  payload.resize(payload.size() - crc_len);
  return (this->accept_header_crc_ && this->calculate_crc_(payload.data(), payload.size(), true) == received_crc) ||
         (this->accept_payload_crc_ && this->calculate_crc_(payload.data(), payload.size(), false) == received_crc);
}

uint16_t RS485FrameHub::calculate_crc_(const uint8_t *data, size_t len, bool include_header) const {
  const uint8_t header[2] = {this->dle_, this->stx_};
  const size_t header_len = include_header ? sizeof(header) : 0;
  switch (this->crc_type_) {
    case CRC_TYPE_SUM8:
      return sum_bytes(data, len, sum_bytes(header, header_len));
    case CRC_TYPE_SUM16_BIG_ENDIAN:
    case CRC_TYPE_SUM16_LITTLE_ENDIAN:
      return sum16_bytes(data, len, sum16_bytes(header, header_len));
    case CRC_TYPE_XOR8:
      return xor_bytes(data, len, xor_bytes(header, header_len));
    case CRC_TYPE_CRC16_MODBUS:
      return crc16(data, static_cast<uint16_t>(len), crc16(header, static_cast<uint16_t>(header_len)));
    case CRC_TYPE_NONE:
      break;
  }
  return 0;
}

size_t RS485FrameHub::crc_length_() const {
  switch (this->crc_type_) {
    case CRC_TYPE_NONE:
      return 0;
    case CRC_TYPE_SUM8:
    case CRC_TYPE_XOR8:
      return 1;
    default:
      return 2;
  }
}

void RS485FrameHub::append_escaped_(uint8_t byte, std::vector<uint8_t> &out) const {
  out.push_back(byte);
  if (byte == this->dle_)
    out.push_back(this->escape_marker_);
}

void RS485FrameHub::build_frame_(const std::vector<uint8_t> &payload, std::vector<uint8_t> &out) {
  out.clear();
  out.push_back(this->dle_);
  out.push_back(this->stx_);
  for (uint8_t b : payload)
    this->append_escaped_(b, out);
  // The CRC covers the unescaped payload; its own bytes are then escaped like payload bytes.
  const size_t crc_len = this->crc_length_();
  if (crc_len > 0) {
    uint16_t crc = this->calculate_crc_(payload.data(), payload.size(), this->tx_crc_variant_ == CRC_HEADER_INCLUSIVE);
    if (crc_len == 1) {
      this->append_escaped_(crc & 0xFF, out);
    } else if (this->crc_little_endian_()) {
      this->append_escaped_(crc & 0xFF, out);
      this->append_escaped_(crc >> 8, out);
    } else {
      this->append_escaped_(crc >> 8, out);
      this->append_escaped_(crc & 0xFF, out);
    }
  }
  out.push_back(this->dle_);
  out.push_back(this->etx_);
}

void RS485FrameHub::maybe_tx_(uint32_t now) {
  if (this->sniffer_only_ || this->tx_start_pending_ || (this->tx_queue_count_ == 0 && this->idle_frame_len_ == 0))
    return;
  if (this->tx_gate_mode_ == TX_GATE_IDLE_GAP && this->has_activity_ &&
      now - this->last_activity_time_ >= this->tx_idle_gap_)
    this->send_next_(now);
  if (this->tx_gate_mode_ == TX_GATE_FIXED_DELAY &&
      (!this->has_tx_ever_ || now - this->last_tx_time_ >= this->tx_fixed_interval_))
    this->send_next_(now);
}

void RS485FrameHub::queue_pop_front_() {
  this->tx_queue_[this->tx_queue_head_].clear();
  this->tx_queue_head_ = (this->tx_queue_head_ + 1) % this->max_queue_size_;
  this->tx_queue_count_--;
}

void RS485FrameHub::send_next_(uint32_t now) {
  if (this->sniffer_only_ || this->tx_start_pending_)
    return;
  const bool idle = this->tx_queue_count_ == 0;
  std::vector<uint8_t> *frame = &this->tx_queue_[this->tx_queue_head_];
  if (idle) {
    if (this->idle_frame_len_ == 0)
      return;
    this->tx_payload_buf_.resize(this->idle_frame_len_);
    progmem_memcpy(this->tx_payload_buf_.data(), this->idle_frame_, this->idle_frame_len_);
    this->build_frame_(this->tx_payload_buf_, this->tx_frame_buf_);
    frame = &this->tx_frame_buf_;
  }
  if (this->tx_gate_delay_ > 0) {
    std::swap(this->pending_tx_frame_, *frame);
    if (!idle)
      this->queue_pop_front_();
    this->tx_start_at_ = now + this->tx_gate_delay_;
    this->tx_start_pending_ = true;
    this->pending_is_idle_ = idle;
    return;
  }
  this->transmit_(*frame, now, idle);
  if (!idle)
    this->queue_pop_front_();
}

void RS485FrameHub::transmit_(std::vector<uint8_t> &frame, uint32_t now, bool idle) {
  this->write_frame_(frame);
  this->last_tx_time_ = now;
  this->last_activity_time_ = now;
  this->has_activity_ = true;
  this->has_tx_ever_ = true;
  if (!idle)
    this->commands_sent_++;
  this->pending_is_idle_ = false;
}

void RS485FrameHub::write_frame_(const std::vector<uint8_t> &frame) {
  this->write_array(frame);
#if defined(USE_ESP8266) || defined(USE_RP2)
  // These UARTs do not drive flow_control_pin, so an auto-DE transceiver needs the bytes on the
  // wire before returning; ESP32 drives DE in hardware RS485 mode.
  this->flush();
#endif
  if (this->dump_frames_)
    this->log_frame_("TX", frame.data(), frame.size());
}

void RS485FrameHub::log_frame_(const char *dir, const uint8_t *data, size_t len) {
  format_hex_to(this->hex_log_buf_.get(), this->hex_log_buf_size_, data, len);
  ESP_LOGD(TAG, "%s %s", dir, this->hex_log_buf_.get());
}

}  // namespace esphome::rs485_frame

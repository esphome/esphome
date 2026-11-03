#include "rs485_frame.h"

#include "esphome/core/application.h"

#include <algorithm>
#include <cinttypes>

namespace esphome::rs485_frame {

ESPHOME_LOG_TAG(TAG, "rs485_frame");

static const char *crc_type_str(CrcType t) {
  switch (t) {
    case CRC_TYPE_NONE:
      return "none";
    case CRC_TYPE_SUM8:
      return "sum8";
    case CRC_TYPE_SUM16_BIG_ENDIAN:
      return "sum16_big_endian";
    case CRC_TYPE_SUM16_LITTLE_ENDIAN:
      return "sum16_little_endian";
    case CRC_TYPE_XOR8:
      return "xor8";
    case CRC_TYPE_CRC16_MODBUS:
      return "crc16_modbus";
    default:
      return "unknown";
  }
}

static const char *tx_gate_mode_str(TxGateMode m) {
  switch (m) {
    case TX_GATE_FRAME_TRIGGER:
      return "frame_trigger";
    case TX_GATE_IDLE_GAP:
      return "idle_gap";
    case TX_GATE_FIXED_DELAY:
      return "fixed_delay";
    default:
      return "unknown";
  }
}

static const char *queue_policy_str(QueuePolicy p) {
  switch (p) {
    case QUEUE_REPLACE_LATEST:
      return "replace_latest";
    case QUEUE_FIFO:
      return "fifo";
    default:
      return "unknown";
  }
}

bool RS485FrameTrigger::matches(const std::vector<uint8_t> &payload) const {
  // Empty prefix list = match every frame (documented `frame_type: []` behavior). An
  // individual prefix inside a multi-alternate list can never itself be empty — the
  // schema (validate_frame_type_or_list) rejects that at config-validate time, since it
  // would otherwise silently make the whole trigger match-all instead of doing nothing.
  if (this->frame_types_.empty())
    return true;
  for (const auto &prefix : this->frame_types_) {
    if (payload.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), payload.begin()))
      return true;
  }
  return false;
}

void RS485FrameHub::setup() {
  // Pre-allocate scratch buffers to avoid per-frame heap churn on the main receive path.
  const size_t tx_slot_capacity = this->max_frame_length_ * 2 + FRAME_OVERHEAD_BYTES;
  this->raw_frame_ = std::make_unique<uint8_t[]>(this->max_frame_length_);
  this->rx_payload_.reserve(this->max_frame_length_);
  // Reserve the worst case any command_format can produce — not just this hub's own
  // configured preamble/value_element_bytes/postamble. A button's command_format override
  // (queue_command_with_format) is independent of the hub's and can be larger, even when
  // the hub itself has no command_format at all (e.g. a button overriding on a raw-only
  // hub). Bounded by the schema's MAX_* constants and the max value_element_bytes (4), so
  // this is a small, fixed reservation regardless of what's actually configured.
  constexpr size_t max_value_element_bytes = 4;
  const size_t key_payload_cap =
      MAX_COMMAND_PREAMBLE_LEN + max_value_element_bytes * MAX_COMMAND_VALUES + MAX_COMMAND_POSTAMBLE_LEN;
  this->tx_payload_buf_.reserve(key_payload_cap);
  // send_assembly_buf_ holds an assembled frame_type+payload before framing; it can never
  // legitimately exceed max_frame_length_ (queue_raw_frame rejects anything larger).
  this->send_assembly_buf_.reserve(this->max_frame_length_);
  this->tx_frame_buf_.reserve(tx_slot_capacity);
  this->pending_tx_frame_.reserve(tx_slot_capacity);
  // Ring buffer: pre-size and pre-reserve each slot so enqueue only swaps, never allocates.
  this->tx_queue_.resize(this->max_queue_size_);
  for (auto &slot : this->tx_queue_)
    slot.reserve(tx_slot_capacity);
  // Hex-text log buffer sized to the worst-case TX frame (2 hex chars per byte + NUL).
  this->hex_log_buf_size_ = tx_slot_capacity * 2 + 1;
  this->hex_log_buf_ = std::make_unique<char[]>(this->hex_log_buf_size_);
}

void RS485FrameHub::loop() {
  const uint32_t now = App.get_loop_component_start_time();

  this->read_uart_(now);

  // Reset receive state if a partial frame has been sitting on the bus longer than the
  // intra-frame timeout. Without this, a cable pull or noise burst mid-frame would
  // permanently stall reception until the next valid frame terminator arrived.
  if (this->in_frame_ && this->in_frame_timeout_ms_ > 0 && now - this->last_rx_time_ >= this->in_frame_timeout_ms_) {
    ESP_LOGW(TAG, "Intra-frame timeout — resetting receive state");
    this->record_discard_();
    this->in_frame_ = false;
    this->after_dle_ = false;
    this->raw_len_ = 0;
    // Reset previous_byte_ so a stale DLE before the timeout can't combine with the next STX
    // to spuriously start a new frame on the first byte received after recovery.
    this->previous_byte_ = 0;
  }

  this->maybe_tx_(now);
  // Unsigned subtraction wraps correctly so this comparison handles the 49-day millis rollover.
  if (this->tx_start_pending_ && now - this->tx_start_at_ < 0x80000000UL) {
    this->tx_start_pending_ = false;
    this->write_frame_(this->pending_tx_frame_);
    this->mark_transmitted_(now);
    if (!this->pending_is_idle_)
      this->commands_sent_++;
    this->pending_is_idle_ = false;
  }
}

void RS485FrameHub::dump_config() {
  // StaticVector caps the size at MAX_FRAME_TYPE_LEN, so no run-time bound needed.
  char gate_hex[format_hex_size(MAX_FRAME_TYPE_LEN)];
  format_hex_to(gate_hex, this->tx_gate_frame_type_.data(), this->tx_gate_frame_type_.size());
  // escape_marker_ == dle_ means doubling mode (a literal DLE is stuffed as DLE DLE);
  // otherwise the marker is the dedicated escape byte emitted after a DLE.
  const char *esc_desc_ptr;
  char esc_desc_buf[16];
  if (this->escape_marker_ == this->dle_) {
    esc_desc_ptr = "double (DLE DLE)";
  } else {
    snprintf(esc_desc_buf, sizeof(esc_desc_buf), "byte 0x%02x", this->escape_marker_);
    esc_desc_ptr = esc_desc_buf;
  }
  // Consolidated multi-line ESP_LOGCONFIG (matches modbus_server style) to save flash.
  ESP_LOGCONFIG(TAG,
                "RS485 Frame:\n"
                "  Framing: DLE=0x%02x STX=0x%02x ETX=0x%02x, escape: %s\n"
                "  CRC type: %s, TX CRC variant: %s, accept header CRC: %s, accept payload CRC: %s\n"
                "  TX gate: %s, gate frame: %s, gate delay: %" PRIu32 "ms\n"
                "  TX idle gap: %" PRIu32 "ms, TX interval: %" PRIu32 "ms\n"
                "  Queue policy: %s, queue size: %" PRIu32 "\n"
                "  Max frame length: %" PRIu32 ", frame timeout: %" PRIu32 "ms\n"
                "  Sniffer only: %s, dump frames: %s",
                this->dle_, this->stx_, this->etx_, esc_desc_ptr, crc_type_str(this->crc_type_),
                this->tx_crc_variant_ == CRC_HEADER_INCLUSIVE ? LOG_STR_LITERAL("header_inclusive")
                                                              : LOG_STR_LITERAL("payload_only"),
                YESNO(this->accept_header_crc_), YESNO(this->accept_payload_crc_),
                tx_gate_mode_str(this->tx_gate_mode_), gate_hex, this->tx_gate_delay_, this->tx_idle_gap_,
                this->tx_fixed_interval_, queue_policy_str(this->queue_policy_), this->max_queue_size_,
                this->max_frame_length_, this->in_frame_timeout_ms_, YESNO(this->sniffer_only_),
                YESNO(this->dump_frames_));
  if (this->has_command_format_) {
    ESP_LOGCONFIG(TAG, "  Command format: preamble=%zu bytes, element_bytes=%u, endian=%s, postamble=%zu bytes",
                  this->cmd_preamble_.size(), this->cmd_value_element_bytes_,
                  this->cmd_big_endian_ ? LOG_STR_LITERAL("big") : LOG_STR_LITERAL("little"),
                  this->cmd_postamble_.size());
    if (this->has_idle_command_) {
      ESP_LOGCONFIG(TAG, "  Idle command: 0x%08" PRIx32, this->idle_command_);
    }
  }
}

void RS485FrameHub::set_framing(uint8_t dle, uint8_t stx, uint8_t etx, uint8_t escape_marker) {
  this->dle_ = dle;
  this->stx_ = stx;
  this->etx_ = etx;
  this->escape_marker_ = escape_marker;
}

bool RS485FrameHub::queue_command_values(const uint32_t *commands, size_t count) {
  if (this->sniffer_only_) {
    ESP_LOGW(TAG, "Ignoring command because sniffer_only is enabled");
    this->command_drops_++;
    return false;
  }
  // Without a command_format:, there is no configured preamble/element width/endianness/
  // postamble to encode against -- cmd_value_element_bytes_/cmd_big_endian_ would silently
  // fall back to their bare-field defaults (4-byte big-endian, no preamble or postamble)
  // instead of reflecting what the bus actually expects.
  if (!this->has_command_format_) {
    ESP_LOGW(TAG, "queue_command_value(s) called with no command_format: configured; dropping");
    this->command_drops_++;
    return false;
  }
  this->build_key_payload_(commands, count, this->cmd_value_element_bytes_, this->tx_payload_buf_);
  // A command_format whose encoded payload exceeds max_frame_length_ would overflow the
  // pre-reserved TX buffers in build_frame_. Drop rather than reallocate after setup().
  if (this->tx_payload_buf_.size() > this->max_frame_length_) {
    ESP_LOGW(TAG, "Command payload (%zu) exceeds max_frame_length (%" PRIu32 "); dropping",
             this->tx_payload_buf_.size(), this->max_frame_length_);
    this->command_drops_++;
    return false;
  }
  this->build_frame_(this->tx_payload_buf_, this->tx_frame_buf_);
  return this->enqueue_frame_();
}

bool RS485FrameHub::enqueue_frame_() {
  if (this->queue_policy_ == QUEUE_REPLACE_LATEST) {
    if (this->queue_size_() > 0 || this->tx_start_pending_) {
      ESP_LOGW(TAG, "Replacing pending rs485_frame frame before it was transmitted");
      this->command_drops_++;
    }
    if (this->tx_start_pending_) {
      // Swap the new frame into pending_tx_frame_ — both are pre-reserved, no allocation.
      std::swap(this->pending_tx_frame_, this->tx_frame_buf_);
      this->pending_is_idle_ = false;
      return true;
    }
    // Reset ring buffer logical state; slot capacity is preserved.
    this->tx_queue_head_ = 0;
    this->tx_queue_tail_ = 0;
    this->tx_queue_count_ = 0;
  } else {
    if (this->tx_queue_count_ + (this->tx_start_pending_ ? 1 : 0) >= this->max_queue_size_) {
      ESP_LOGW(TAG, "rs485_frame frame queue full; dropping frame");
      this->command_drops_++;
      return false;
    }
  }
  // Swap tx_frame_buf_ into the pre-reserved tail slot — no heap allocation.
  // After the swap, tx_frame_buf_ holds the slot's old content (empty, capacity intact).
  std::swap(this->tx_queue_[this->tx_queue_tail_], this->tx_frame_buf_);
  this->tx_queue_tail_ = (this->tx_queue_tail_ + 1) % this->max_queue_size_;
  this->tx_queue_count_++;
  return true;
}

void RS485FrameHub::read_uart_(uint32_t now) {
  this->read_available([this, now](uint8_t byte) { this->handle_rx_byte_(byte, now); });
}

void RS485FrameHub::handle_rx_byte_(uint8_t byte, uint32_t now) {
  this->last_rx_time_ = now;
  this->last_activity_time_ = now;
  this->has_activity_ = true;
  if (!this->in_frame_) {
    if (this->previous_byte_ == this->dle_ && byte == this->stx_) {
      this->in_frame_ = true;
      this->after_dle_ = false;
      this->raw_frame_[0] = this->dle_;
      this->raw_frame_[1] = this->stx_;
      this->raw_len_ = 2;
    }
    this->previous_byte_ = byte;
    return;
  }

  if (this->raw_len_ >= this->max_frame_length_) {
    ESP_LOGW(TAG, "Frame exceeded max_frame_length");
    this->record_discard_();
    this->in_frame_ = false;
    this->after_dle_ = false;
    this->raw_len_ = 0;
    this->previous_byte_ = byte;
    return;
  }
  this->raw_frame_[this->raw_len_++] = byte;

  // Escape-aware terminator detection. Mid-frame, every DLE is resolved by the byte that
  // follows it: DLE+ETX ends the frame, DLE+escape_marker is a stuffed literal DLE
  // (escape_marker_ == DLE in doubling mode, == the escape byte otherwise), and any other
  // successor is a protocol violation that validate_frame_ rejects. Tracking after_dle_ is
  // required for doubling mode: peeking only at the previous raw byte miscounts adjacent
  // DLEs (a DLE DLE literal just before a DLE ETX terminator, or a literal DLE followed by
  // a literal ETX data byte).
  if (this->after_dle_) {
    // The pending DLE is now resolved by this byte; it cannot itself open a new escape
    // sequence (in doubling mode the marker equals DLE but is consumed here as the partner
    // of the pending DLE, not as a fresh DLE).
    this->after_dle_ = false;
    if (byte == this->etx_) {
      this->process_raw_frame_(now);
      this->in_frame_ = false;
      this->raw_len_ = 0;
    }
  } else if (byte == this->dle_) {
    this->after_dle_ = true;
  }
  // Maintain previous_byte_ for every in-frame byte too. Without this, a normal frame
  // terminator (DLE ETX) leaves previous_byte_ stale at the value seen just before the
  // frame began (typically DLE), so a bare STX immediately after the terminator would
  // spuriously start a new frame.
  this->previous_byte_ = byte;
}

void RS485FrameHub::process_raw_frame_(uint32_t now) {
  if (!this->validate_frame_()) {
    this->crc_failures_++;
    return;
  }

  this->frames_received_++;
  this->update_last_frame_type_();
  if (this->dump_frames_) {
    // Reuse the setup-time allocated hex_log_buf_ to avoid per-frame heap allocation
    // that the std::vector-returning hex formatter would incur. Buffer is sized for the
    // worst-case TX frame, which comfortably fits any RX raw_frame_ too. Logs the full wire
    // frame (framing + CRC), matching write_frame_'s TX log below -- "frame" means all bits.
    format_hex_to(this->hex_log_buf_.get(), this->hex_log_buf_size_, this->raw_frame_.get(), this->raw_len_);
    ESP_LOGD(TAG, "RX %s", this->hex_log_buf_.get());
  }

  if (this->frame_type_equals_(this->rx_payload_, this->tx_gate_frame_type_)) {
    if (this->last_ka_seen_)
      this->last_keepalive_ms_ = now - this->last_ka_time_;
    this->last_ka_seen_ = true;
    this->last_ka_time_ = now;
    if (this->tx_gate_mode_ == TX_GATE_FRAME_TRIGGER)
      this->send_next_(now);
  }

#ifdef RS485_FRAME_TRIGGER_COUNT
  for (auto *trigger : this->triggers_) {
    if (trigger->matches(this->rx_payload_))
      trigger->trigger(this->rx_payload_);
  }
#endif
}

bool RS485FrameHub::validate_frame_() {
  const uint8_t *frame = this->raw_frame_.get();
  const size_t len = this->raw_len_;
  // The constant 6 is the no-CRC minimum (DLE STX + 2 frame_type bytes + DLE ETX); the CRC
  // length is checked after unescaping below.
  if (len < 6 || frame[0] != this->dle_ || frame[1] != this->stx_ || frame[len - 2] != this->dle_ ||
      frame[len - 1] != this->etx_)
    return false;

  // rx_payload_ is what on_frame: triggers and the gate matcher see: the unescaped bytes
  // between DLE+STX and DLE+ETX, starting with frame_type (payload[0..N-1]) and with the CRC
  // removed below. Its capacity is max_frame_length_, which len never exceeds.
  auto &payload = this->rx_payload_;
  payload.clear();
  for (size_t i = 2; i + 2 < len; i++) {
    uint8_t b = frame[i];
    if (b == this->dle_) {
      // A DLE inside the frame must be followed by escape_marker_ (the escape byte, or DLE
      // itself in doubling mode); anything else is a protocol violation.
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
  bool header_ok =
      this->accept_header_crc_ && this->calculate_crc_(payload.data(), payload.size(), true) == received_crc;
  bool payload_ok =
      this->accept_payload_crc_ && this->calculate_crc_(payload.data(), payload.size(), false) == received_crc;
  return header_ok || payload_ok;
}

uint16_t RS485FrameHub::calculate_crc_(const uint8_t *data, size_t len, bool include_header) const {
  // The header-inclusive variant runs DLE+STX through the same helper first, then the payload.
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
    case CRC_TYPE_SUM16_BIG_ENDIAN:
    case CRC_TYPE_SUM16_LITTLE_ENDIAN:
    case CRC_TYPE_CRC16_MODBUS:
      return 2;
  }
  // A new CrcType must add a case above AND update calculate_crc_() to keep them in sync.
  return 2;
}

void RS485FrameHub::append_escaped_(uint8_t byte, std::vector<uint8_t> &out) const {
  out.push_back(byte);
  if (byte == this->dle_)
    out.push_back(this->escape_marker_);
}

void RS485FrameHub::build_frame_(const std::vector<uint8_t> &payload, std::vector<uint8_t> &out) {
  // out is reserved for max_frame_length_ * 2 + FRAME_OVERHEAD_BYTES (every byte escaped) and
  // callers reject payloads longer than max_frame_length_, so this never reallocates.
  out.clear();
  out.push_back(this->dle_);
  out.push_back(this->stx_);
  for (uint8_t b : payload)
    this->append_escaped_(b, out);

  size_t crc_len = this->crc_length_();
  if (crc_len > 0) {
    // CRC is computed over the unescaped payload, then the CRC bytes are escaped like any other
    // payload byte. Computing over escaped bytes is a common DLE-protocol bug; do not change order.
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

namespace {

// Shared by build_key_payload_ (hub's own command_format) and queue_command_with_format (a
// button's per-button command_format override): preamble bytes, then one or more value fields
// each element_bytes (1, 2, or 4) serialised big- or little-endian back-to-back, then postamble
// bytes. Preamble/postamble are pointer/length pairs rather than std::vector<uint8_t> so a
// caller backed by a StaticVector can pass its own storage through with no allocation.
void encode_command_payload(const uint32_t *commands, size_t count, uint8_t element_bytes, bool big_endian,
                            const uint8_t *preamble, size_t preamble_len, const uint8_t *postamble,
                            size_t postamble_len, std::vector<uint8_t> &out) {
  out.clear();
  for (size_t i = 0; i < preamble_len; i++)
    out.push_back(preamble[i]);
  for (size_t i = 0; i < count; i++) {
    uint32_t command = commands[i];
    if (big_endian) {
      for (int byte = static_cast<int>(element_bytes) - 1; byte >= 0; byte--)
        out.push_back((command >> (byte * 8)) & 0xFF);
    } else {
      for (uint8_t byte = 0; byte < element_bytes; byte++)
        out.push_back((command >> (byte * 8)) & 0xFF);
    }
  }
  for (size_t i = 0; i < postamble_len; i++)
    out.push_back(postamble[i]);
}

}  // namespace

void RS485FrameHub::build_key_payload_(const uint32_t *commands, size_t count, uint8_t element_bytes,
                                       std::vector<uint8_t> &out) const {
  encode_command_payload(commands, count, element_bytes, this->cmd_big_endian_, this->cmd_preamble_.data(),
                         this->cmd_preamble_.size(), this->cmd_postamble_.data(), this->cmd_postamble_.size(), out);
}

bool RS485FrameHub::queue_command_values_with_element_bytes(const uint32_t *commands, size_t count,
                                                            uint8_t element_bytes) {
  if (this->sniffer_only_) {
    ESP_LOGW(TAG, "Ignoring command because sniffer_only is enabled");
    this->command_drops_++;
    return false;
  }
  // Only the element width is overridden here; preamble/endian/postamble still come from the
  // hub's command_format:, so the same guard as queue_command_values() applies.
  if (!this->has_command_format_) {
    ESP_LOGW(TAG, "queue_command_values_with_element_bytes called with no command_format: configured; dropping");
    this->command_drops_++;
    return false;
  }
  this->build_key_payload_(commands, count, element_bytes, this->tx_payload_buf_);
  if (this->tx_payload_buf_.size() > this->max_frame_length_) {
    ESP_LOGW(TAG, "Command payload (%zu) exceeds max_frame_length (%" PRIu32 "); dropping",
             this->tx_payload_buf_.size(), this->max_frame_length_);
    this->command_drops_++;
    return false;
  }
  this->build_frame_(this->tx_payload_buf_, this->tx_frame_buf_);
  return this->enqueue_frame_();
}

void RS485FrameHub::maybe_tx_(uint32_t now) {
  if (this->sniffer_only_ || this->tx_start_pending_)
    return;
  // For idle_gap and fixed_delay modes, fire the gate regardless of queue depth so that
  // idle_command keepalives can be emitted; send_next_() falls through to send_next_idle_()
  // when the queue is empty and an idle command is configured.
  const bool queue_or_idle = this->queue_size_() > 0 || this->has_idle_command_;
  if (!queue_or_idle)
    return;
  // idle_gap fires when the bus has been silent (no RX and no TX) for tx_idle_gap_. Keying
  // off last_activity_time_ (advanced on both RX and our own TX) means an idle keepalive we
  // emit resets the timer — otherwise, on a half-duplex bus where we don't hear our own
  // transmission, the gate would re-fire every loop.
  if (this->tx_gate_mode_ == TX_GATE_IDLE_GAP && this->has_activity_ &&
      now - this->last_activity_time_ >= this->tx_idle_gap_)
    this->send_next_(now);
  // fixed_delay fires on a fixed period. has_tx_ever_ is an explicit "never transmitted"
  // flag so the first fire is not gated, without overloading last_tx_time_ == 0 (which is a
  // legitimate timestamp at boot and across the millis rollover).
  if (this->tx_gate_mode_ == TX_GATE_FIXED_DELAY &&
      (!this->has_tx_ever_ || now - this->last_tx_time_ >= this->tx_fixed_interval_))
    this->send_next_(now);
}

void RS485FrameHub::queue_pop_front_() {
  // Clear the slot content but keep its reserved capacity for the next enqueue swap.
  this->tx_queue_[this->tx_queue_head_].clear();
  this->tx_queue_head_ = (this->tx_queue_head_ + 1) % this->max_queue_size_;
  this->tx_queue_count_--;
}

void RS485FrameHub::write_frame_(const std::vector<uint8_t> &frame) {
  this->write_array(frame);
#if defined(USE_ESP8266) || defined(USE_RP2)
  // ESP8266 and RP2040 are Arduino-framework only (no ESP-IDF variant), and their uart
  // component does not drive flow_control_pin, so users must run auto-DE transceivers (DE
  // follows the TX line state). flush() prevents the function from returning before bytes are
  // physically on the wire so the transceiver does not flip back to RX mid-frame. ESP32 is
  // excluded here even under the Arduino framework: since ESPHome b8cee477fe removed
  // uart_component_esp32_arduino.*, every ESP32 build (Arduino or ESP-IDF) uses
  // IDFUARTComponent, whose hardware RS485 half-duplex mode drives DE/RE from the
  // shift-register-done signal — flush() there would be pure busy-wait and is omitted to keep
  // loop() responsive.
  this->flush();
#endif
  if (this->dump_frames_) {
    // Reuse the setup-time allocated hex_log_buf_ — no heap allocation per TX.
    format_hex_to(this->hex_log_buf_.get(), this->hex_log_buf_size_, frame.data(), frame.size());
    ESP_LOGD(TAG, "TX %s", this->hex_log_buf_.get());
  }
}

void RS485FrameHub::send_next_(uint32_t now) {
  if (this->sniffer_only_ || this->tx_start_pending_)
    return;
  if (this->queue_size_() == 0) {
    if (this->has_idle_command_)
      this->send_next_idle_(now);
    return;
  }

  if (this->tx_gate_delay_ > 0) {
    // Swap the frame into pending_tx_frame_ — both are pre-reserved, no allocation.
    std::swap(this->pending_tx_frame_, this->tx_queue_[this->tx_queue_head_]);
    this->queue_pop_front_();
    this->tx_start_at_ = now + this->tx_gate_delay_;
    this->tx_start_pending_ = true;
    this->pending_is_idle_ = false;
    return;
  }

  this->write_frame_(this->tx_queue_[this->tx_queue_head_]);
  this->queue_pop_front_();
  this->mark_transmitted_(now);
  this->commands_sent_++;
}

void RS485FrameHub::send_next_idle_(uint32_t now) {
  this->build_key_payload_(&this->idle_command_, 1, this->cmd_value_element_bytes_, this->tx_payload_buf_);
  this->build_frame_(this->tx_payload_buf_, this->tx_frame_buf_);
  if (this->tx_gate_delay_ > 0) {
    std::swap(this->pending_tx_frame_, this->tx_frame_buf_);
    this->tx_start_at_ = now + this->tx_gate_delay_;
    this->tx_start_pending_ = true;
    this->pending_is_idle_ = true;
    return;
  }
  this->write_frame_(this->tx_frame_buf_);
  this->mark_transmitted_(now);
  // Idle keepalives are not counted in commands_sent_ — that counter tracks only real HA commands.
}

bool RS485FrameHub::queue_command_with_format(const uint32_t *commands, size_t count, const uint8_t *preamble,
                                              size_t preamble_len, uint8_t value_element_bytes, bool big_endian,
                                              const uint8_t *postamble, size_t postamble_len) {
  if (this->sniffer_only_) {
    ESP_LOGW(TAG, "Ignoring command because sniffer_only is enabled");
    this->command_drops_++;
    return false;
  }
  encode_command_payload(commands, count, value_element_bytes, big_endian, preamble, preamble_len, postamble,
                         postamble_len, this->tx_payload_buf_);
  // Mirrors the overflow guard in queue_command_values(): a button's own command_format
  // override is independent of the hub's, so this checks against max_frame_length_ rather
  // than assuming the reserved capacity (sized for the schema's worst case) is exceeded.
  if (this->tx_payload_buf_.size() > this->max_frame_length_) {
    ESP_LOGW(TAG, "Command payload (%zu) exceeds max_frame_length (%" PRIu32 "); dropping",
             this->tx_payload_buf_.size(), this->max_frame_length_);
    this->command_drops_++;
    return false;
  }
  this->build_frame_(this->tx_payload_buf_, this->tx_frame_buf_);
  return this->enqueue_frame_();
}

bool RS485FrameHub::queue_raw_frame(const std::vector<uint8_t> &payload) {
  if (this->sniffer_only_) {
    ESP_LOGW(TAG, "Ignoring raw frame because sniffer_only is enabled");
    this->command_drops_++;
    return false;
  }
  // Bound the payload to max_frame_length_: build_frame_ writes into pre-reserved buffers
  // sized for that worst case, so an oversized payload would force a post-setup realloc.
  if (payload.size() > this->max_frame_length_) {
    ESP_LOGW(TAG, "Raw frame payload (%zu) exceeds max_frame_length (%" PRIu32 "); dropping", payload.size(),
             this->max_frame_length_);
    this->command_drops_++;
    return false;
  }
  this->build_frame_(payload, this->tx_frame_buf_);
  return this->enqueue_frame_();
}

bool RS485FrameHub::queue_raw_frame(const std::vector<uint8_t> &frame_type, const std::vector<uint8_t> &payload) {
  if (this->sniffer_only_) {
    ESP_LOGW(TAG, "Ignoring raw frame because sniffer_only is enabled");
    this->command_drops_++;
    return false;
  }
  if (frame_type.size() + payload.size() > this->max_frame_length_) {
    ESP_LOGW(TAG, "Raw frame (%zu) exceeds max_frame_length (%" PRIu32 "); dropping",
             frame_type.size() + payload.size(), this->max_frame_length_);
    this->command_drops_++;
    return false;
  }
  // Assemble into the pre-reserved buffer — no per-call allocation.
  this->send_assembly_buf_.clear();
  this->send_assembly_buf_.insert(this->send_assembly_buf_.end(), frame_type.begin(), frame_type.end());
  this->send_assembly_buf_.insert(this->send_assembly_buf_.end(), payload.begin(), payload.end());
  this->build_frame_(this->send_assembly_buf_, this->tx_frame_buf_);
  return this->enqueue_frame_();
}

bool RS485FrameHub::frame_type_equals_(const std::vector<uint8_t> &payload,
                                       const StaticVector<uint8_t, MAX_FRAME_TYPE_LEN> &frame_type) const {
  if (frame_type.empty() || payload.size() < frame_type.size())
    return false;
  return std::equal(frame_type.begin(), frame_type.end(), payload.begin());
}

void RS485FrameHub::update_last_frame_type_() {
  size_t len = std::min(this->rx_payload_.size(), size_t(2));
  format_hex_to(this->last_frame_type_, this->rx_payload_.data(), len);
}

}  // namespace esphome::rs485_frame

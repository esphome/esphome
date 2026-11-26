#include "rs485_frame.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>

namespace esphome::rs485_frame {

ESPHOME_LOG_TAG(TAG, "rs485_frame");

PROGMEM_STRING_TABLE(CrcTypeStrings, "none", "sum8", "sum16_big_endian", "sum16_little_endian", "xor8", "crc16_modbus");
PROGMEM_STRING_TABLE(CrcVariantStrings, "header_inclusive", "payload_only");
PROGMEM_STRING_TABLE(GateModeStrings, "frame_trigger", "idle_gap", "fixed_delay");
PROGMEM_STRING_TABLE(QueuePolicyStrings, "replace_latest", "fifo");

void RS485FrameHub::setup() {
  // idle_gap waits min_silence from boot on a quiet bus; fixed_delay may send right away.
  const uint32_t now = millis();
  this->last_activity_time_ = now;
  this->last_tx_time_ = now - this->tx_gate_interval_;
}

void RS485FrameHub::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  // Checked before draining, since new bytes refresh last_rx_time_ and could complete the stale frame.
  if (this->rx_state_ >= RX_STATE_FRAME && this->in_frame_timeout_ms_ > 0 &&
      now - this->last_rx_time_ >= this->in_frame_timeout_ms_) {
    ESP_LOGV(TAG, "Intra-frame timeout, resetting receive state");
    this->reset_rx_();
  }
  this->read_available([this, now](uint8_t byte) { this->handle_rx_byte_(byte, now); });

  // Transmits only after this pass's RX is drained, so frame_trigger sees any byte that followed the gate frame.
  this->maybe_tx_(now);
  if (this->flags_.tx_start_pending && now - this->tx_delay_start_ >= this->tx_gate_delay_ &&
      this->gate_allows_send_(now)) {
    this->flags_.tx_start_pending = false;
    this->transmit_head_(now);
  }
}

void RS485FrameHub::dump_config() {
  uint8_t gate[MAX_FRAME_TYPE_LEN];
  progmem_memcpy(gate, this->tx_gate_frame_type_, this->tx_gate_frame_type_len_);
  char gate_hex[format_hex_size(MAX_FRAME_TYPE_LEN)];
  format_hex_to(gate_hex, gate, this->tx_gate_frame_type_len_);
  ESP_LOGCONFIG(TAG,
                "RS485 Frame:\n"
                "  Framing: DLE=0x%02x STX=0x%02x ETX=0x%02x, escape=0x%02x\n"
                "  CRC type: %s, TX CRC variant: %s, accept header CRC: %s, accept payload CRC: %s\n"
                "  TX gate: %s, gate frame: %s, gate delay: %ums\n"
                "  TX gate interval: %ums\n"
                "  Queue policy: %s, queue size: %u\n"
                "  Max frame length: %u, frame timeout: %ums\n"
                "  Sniffer only: %s, dump frames: %s",
                this->dle_, this->stx_, this->etx_, this->escape_marker_,
                LOG_STR_ARG(CrcTypeStrings::get_log_str(this->crc_type_, 0)),
                LOG_STR_ARG(CrcVariantStrings::get_log_str(this->tx_crc_variant_, 0)),
                YESNO(this->flags_.accept_header_crc), YESNO(this->flags_.accept_payload_crc),
                LOG_STR_ARG(GateModeStrings::get_log_str(this->tx_gate_mode_, 0)), gate_hex, this->tx_gate_delay_,
                this->tx_gate_interval_, LOG_STR_ARG(QueuePolicyStrings::get_log_str(this->queue_policy_, 0)),
                this->max_queue_size_, this->max_frame_length_, this->in_frame_timeout_ms_,
                YESNO(this->tx_queue_ == nullptr), YESNO(this->flags_.dump_frames));
}

bool RS485FrameHub::queue_raw_frame(std::span<const uint8_t> frame_type, std::span<const uint8_t> payload) {
  if (this->tx_queue_ == nullptr) {
    ESP_LOGW(TAG, "Ignoring frame because sniffer_only is enabled");
    return false;
  }
  const size_t len = frame_type.size() + payload.size();
  if (len < MIN_FRAME_LENGTH || len > this->max_frame_length_) {
    ESP_LOGW(TAG, "Frame length %zu is outside %zu..max_frame_length (%u); dropping", len, MIN_FRAME_LENGTH,
             this->max_frame_length_);
    return false;
  }
  size_t index = this->tx_queue_head_;
  if (this->queue_policy_ == QUEUE_FIFO) {
    if (this->tx_queue_count_ >= this->max_queue_size_) {
      ESP_LOGW(TAG, "Frame queue full; dropping frame");
      return false;
    }
    index += this->tx_queue_count_;
    if (index >= this->max_queue_size_)
      index -= this->max_queue_size_;
  }
  // A UART debug or log callback queued while that slot is being sent.
  if (index == this->tx_sending_slot_) {
    ESP_LOGW(TAG, "Frame queued while the previous frame was being sent; dropping frame");
    return false;
  }
  if (this->queue_policy_ == QUEUE_REPLACE_LATEST) {
    if (this->tx_queue_count_ > 0) {
      ESP_LOGV(TAG, "Replacing pending frame before it was transmitted");
    }
    this->tx_queue_count_ = 0;
  }
  uint8_t *slot = this->tx_slot_(index);
  progmem_memcpy(slot, frame_type.data(), frame_type.size());
  progmem_memcpy(slot + frame_type.size(), payload.data(), payload.size());
  this->encode_crc_(slot, len, this->tx_crc_variant_ == CRC_HEADER_INCLUSIVE, slot + len);
  this->tx_queue_len_[index] = len + this->crc_len_;
  this->tx_queue_count_++;
  return true;
}

void RS485FrameHub::reset_rx_() { this->rx_state_ = RX_STATE_IDLE; }

void RS485FrameHub::handle_rx_byte_(uint8_t byte, uint32_t now) {
  this->last_rx_time_ = now;
  this->last_activity_time_ = now;
  // frame_trigger answers in the gap right after the gate frame; a later byte means another node took it.
  if (this->tx_gate_mode_ == TX_GATE_FRAME_TRIGGER)
    this->flags_.tx_start_pending = false;
  // After a DLE: STX (re)starts a frame, ETX ends it, the escape marker is a literal DLE, anything else drops it.
  const RxState after_other_dle = byte == this->dle_ ? RX_STATE_IDLE_DLE : RX_STATE_IDLE;
  switch (this->rx_state_) {
    case RX_STATE_IDLE:
      this->rx_state_ = after_other_dle;
      return;
    case RX_STATE_FRAME:
      if (byte == this->dle_) {
        this->rx_state_ = RX_STATE_FRAME_DLE;
      } else {
        this->append_rx_(byte);
      }
      return;
    case RX_STATE_IDLE_DLE:
      if (byte == this->stx_) {
        this->rx_state_ = RX_STATE_FRAME;
        this->rx_payload_len_ = 0;
      } else {
        this->rx_state_ = after_other_dle;
      }
      return;
    default:  // RX_STATE_FRAME_DLE
      if (byte == this->stx_) {
        this->rx_state_ = RX_STATE_FRAME;
        this->rx_payload_len_ = 0;
      } else if (byte == this->etx_) {
        this->reset_rx_();
        this->process_frame_(now);
      } else if (byte == this->escape_marker_) {
        this->rx_state_ = RX_STATE_FRAME;
        this->append_rx_(this->dle_);
      } else {
        this->rx_state_ = after_other_dle;
      }
      return;
  }
}

void RS485FrameHub::append_rx_(uint8_t byte) {
  if (this->rx_payload_len_ == this->max_frame_length_ + this->crc_len_) {
    ESP_LOGW(TAG, "Frame exceeded max_frame_length; resetting");
    this->reset_rx_();
    return;
  }
  this->rx_payload_[this->rx_payload_len_++] = byte;
}

void RS485FrameHub::process_frame_(uint32_t now) {
  if (this->rx_payload_len_ < MIN_FRAME_LENGTH + this->crc_len_)
    return;
  this->rx_payload_len_ -= this->crc_len_;
  if (!this->check_crc_())
    return;
  if (this->flags_.dump_frames)
    this->log_frame_(LOG_STR("RX"), this->rx_payload_, this->rx_payload_len_ + this->crc_len_);

  const std::span<const uint8_t> payload(this->rx_payload_, this->rx_payload_len_);
  // Released after the callbacks, so a reply they queue goes out in this gap.
  this->frame_callback_.call(payload);
  if (this->tx_gate_mode_ == TX_GATE_FRAME_TRIGGER &&
      frame_has_prefix(payload, this->tx_gate_frame_type_, this->tx_gate_frame_type_len_))
    this->send_next_(now);
}

bool RS485FrameHub::check_crc_() const {
  return this->crc_len_ == 0 || (this->flags_.accept_header_crc && this->crc_matches_(true)) ||
         (this->flags_.accept_payload_crc && this->crc_matches_(false));
}

bool RS485FrameHub::crc_matches_(bool include_header) const {
  uint8_t crc[2]{};
  this->encode_crc_(this->rx_payload_, this->rx_payload_len_, include_header, crc);
  return memcmp(crc, this->rx_payload_ + this->rx_payload_len_, this->crc_len_) == 0;
}

void RS485FrameHub::encode_crc_(const uint8_t *data, size_t len, bool include_header, uint8_t *out) const {
  const uint8_t header[2] = {this->dle_, this->stx_};
  const size_t header_len = include_header ? sizeof(header) : 0;
  switch (this->crc_type_) {
    case CRC_TYPE_SUM8:
      out[0] = sum_bytes(data, len, sum_bytes(header, header_len));
      return;
    case CRC_TYPE_XOR8:
      out[0] = xor_bytes(data, len, xor_bytes(header, header_len));
      return;
    case CRC_TYPE_SUM16_BIG_ENDIAN:
      put_be16(out, sum16_bytes(data, len, sum16_bytes(header, header_len)));
      return;
    case CRC_TYPE_SUM16_LITTLE_ENDIAN:
      put_le16(out, sum16_bytes(data, len, sum16_bytes(header, header_len)));
      return;
    case CRC_TYPE_CRC16_MODBUS:
      put_le16(out, crc16(data, static_cast<uint16_t>(len), crc16(header, static_cast<uint16_t>(header_len))));
      return;
    default:
      return;
  }
}

void RS485FrameHub::maybe_tx_(uint32_t now) {
  if (this->tx_queue_count_ == 0 || this->flags_.tx_start_pending || this->tx_gate_mode_ == TX_GATE_FRAME_TRIGGER)
    return;
  if (now - this->last_tx_time_ < this->last_tx_wire_ms_ + this->tx_gate_interval_)
    return;
  if (this->tx_gate_mode_ == TX_GATE_FIXED_DELAY || this->bus_silent_(now))
    this->send_next_(now);
}

void RS485FrameHub::send_next_(uint32_t now) {
  if (this->flags_.tx_start_pending || this->tx_queue_count_ == 0)
    return;
  this->tx_delay_start_ = now;
  this->flags_.tx_start_pending = true;
}

void RS485FrameHub::transmit_head_(uint32_t now) {
  const uint8_t slot = this->tx_queue_head_;
  const uint8_t *frame = this->tx_slot_(slot);
  const size_t len = this->tx_queue_len_[slot];
  // Dequeued before any callback runs, so a frame they queue is kept; the slot stays reserved until sent and logged.
  if (++this->tx_queue_head_ == this->max_queue_size_)
    this->tx_queue_head_ = 0;
  this->tx_queue_count_--;
  this->tx_sending_slot_ = slot;
  static constexpr size_t TX_CHUNK_SIZE = 32;
  uint8_t chunk[TX_CHUNK_SIZE];
  size_t pos = 0;
  size_t wire_len = 0;
  auto put = [&](uint8_t byte) {
    chunk[pos++] = byte;
    if (pos == TX_CHUNK_SIZE) {
      this->write_array(chunk, pos);
      wire_len += pos;
      pos = 0;
    }
  };
  this->encode_wire_(frame, len, put);
  if (pos > 0)
    this->write_array(chunk, pos);
  wire_len += pos;
#if defined(USE_ESP8266) || defined(USE_RP2)
  // No hardware DE control here, so an auto-DE transceiver needs the bytes out before returning.
  this->flush();
#endif
  if (this->flags_.dump_frames)
    this->log_frame_(LOG_STR("TX"), frame, len);
  this->tx_sending_slot_ = NO_SLOT;
  this->last_tx_time_ = now;
  this->last_tx_wire_ms_ = this->wire_time_ms_(wire_len);
  this->last_activity_time_ = now;
}

uint16_t RS485FrameHub::wire_time_ms_(size_t len) const {
  const uint32_t baud = this->parent_->get_baud_rate();
  if (baud == 0)
    return 0;
  const uint32_t bits = 1 + this->parent_->get_data_bits() +
                        (this->parent_->get_parity() != uart::UART_CONFIG_PARITY_NONE ? 1 : 0) +
                        this->parent_->get_stop_bits();
  return std::min<uint32_t>((len * bits * 1000 + baud - 1) / baud, UINT16_MAX);
}

void RS485FrameHub::log_frame_(const LogString *dir, const uint8_t *data, size_t len) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
  static constexpr size_t BYTES_PER_LINE = 32;
  uint8_t line[BYTES_PER_LINE];
  char hex[format_hex_size(BYTES_PER_LINE)];
  size_t pos = 0;
  auto log_line = [&]() {
    format_hex_to(hex, line, pos);
    ESP_LOGD(TAG, "%s %s", LOG_STR_ARG(dir), hex);
    pos = 0;
  };
  this->encode_wire_(data, len, [&](uint8_t byte) {
    line[pos++] = byte;
    if (pos == BYTES_PER_LINE)
      log_line();
  });
  if (pos > 0)
    log_line();
#endif
}

}  // namespace esphome::rs485_frame

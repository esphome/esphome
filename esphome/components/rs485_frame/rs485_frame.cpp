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
  // A partial frame left by a cable pull or noise burst would otherwise stall reception. Checked before
  // draining, since the new bytes refresh last_rx_time_ and could complete the stale frame.
  if (this->in_frame_ && this->in_frame_timeout_ms_ > 0 && now - this->last_rx_time_ >= this->in_frame_timeout_ms_) {
    ESP_LOGW(TAG, "Intra-frame timeout, resetting receive state");
    this->reset_rx_();
  }
  this->read_available([this, now](uint8_t byte) { this->handle_rx_byte_(byte, now); });

  // Transmits only after this pass's RX is drained, so frame_trigger sees any byte that followed the gate frame.
  this->maybe_tx_(now);
  // idle_gap also needs the bus to still be silent, since RX during the delay may be a frame in progress. Bytes
  // that arrived while an on_frame callback ran are still waiting; the next pass reads them before sending.
  if (this->tx_start_pending_ && now - this->tx_delay_start_ >= this->tx_gate_delay_ &&
      (this->tx_gate_mode_ != TX_GATE_IDLE_GAP || this->bus_silent_(now)) &&
      (this->tx_gate_mode_ == TX_GATE_FIXED_DELAY || this->available() == 0)) {
    this->tx_start_pending_ = false;
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
                "  TX gate: %s, gate frame: %s, gate delay: %" PRIu32 "ms\n"
                "  TX gate interval: %" PRIu32 "ms\n"
                "  Queue policy: %s, queue size: %u\n"
                "  Max frame length: %u, frame timeout: %" PRIu32 "ms\n"
                "  Sniffer only: %s, dump frames: %s",
                this->dle_, this->stx_, this->etx_, this->escape_marker_,
                LOG_STR_ARG(CrcTypeStrings::get_log_str(this->crc_type_, 0)),
                LOG_STR_ARG(CrcVariantStrings::get_log_str(this->tx_crc_variant_, 0)), YESNO(this->accept_header_crc_),
                YESNO(this->accept_payload_crc_), LOG_STR_ARG(GateModeStrings::get_log_str(this->tx_gate_mode_, 0)),
                gate_hex, this->tx_gate_delay_, this->tx_gate_interval_,
                LOG_STR_ARG(QueuePolicyStrings::get_log_str(this->queue_policy_, 0)), this->max_queue_size_,
                this->max_frame_length_, this->in_frame_timeout_ms_, YESNO(this->tx_queue_ == nullptr),
                YESNO(this->dump_frames_));
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
  if (this->queue_policy_ == QUEUE_REPLACE_LATEST) {
    // The only slot; a frame waiting out the gate delay is replaced and keeps its start time.
    if (this->tx_queue_count_ > 0) {
      ESP_LOGW(TAG, "Replacing pending frame before it was transmitted");
    }
    this->tx_queue_count_ = 0;
  } else if (this->tx_queue_count_ >= this->max_queue_size_) {
    ESP_LOGW(TAG, "Frame queue full; dropping frame");
    return false;
  } else {
    index += this->tx_queue_count_;
    if (index >= this->max_queue_size_)
      index -= this->max_queue_size_;
  }
  uint8_t *slot = this->tx_slot_(index);
  progmem_memcpy(slot, frame_type.data(), frame_type.size());
  progmem_memcpy(slot + frame_type.size(), payload.data(), payload.size());
  this->encode_crc_(slot, len, this->tx_crc_variant_ == CRC_HEADER_INCLUSIVE, slot + len);
  this->tx_queue_len_[index] = len + this->crc_len_;
  this->tx_queue_count_++;
  return true;
}

void RS485FrameHub::reset_rx_() {
  this->in_frame_ = false;
  this->after_dle_ = false;
}

void RS485FrameHub::handle_rx_byte_(uint8_t byte, uint32_t now) {
  this->last_rx_time_ = now;
  this->last_activity_time_ = now;
  // frame_trigger answers in the gap right after the gate frame; a later byte means another node took it.
  if (this->tx_gate_mode_ == TX_GATE_FRAME_TRIGGER)
    this->tx_start_pending_ = false;
  if (!this->after_dle_) {
    if (byte == this->dle_) {
      this->after_dle_ = true;
    } else if (this->in_frame_) {
      this->append_rx_(byte);
    }
    return;
  }
  // The byte after a DLE: STX starts a frame (also inside one, since the escape marker is never
  // STX), ETX ends it, the escape marker makes a literal DLE and anything else drops the frame.
  this->after_dle_ = false;
  if (byte == this->stx_) {
    this->in_frame_ = true;
    this->rx_payload_len_ = 0;
  } else if (!this->in_frame_) {
    this->after_dle_ = byte == this->dle_;
  } else if (byte == this->etx_) {
    this->reset_rx_();
    this->process_frame_(now);
  } else if (byte == this->escape_marker_) {
    this->append_rx_(this->dle_);
  } else {
    this->reset_rx_();
    this->after_dle_ = byte == this->dle_;
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
  if (this->dump_frames_)
    this->log_frame_(LOG_STR("RX"), this->rx_payload_, this->rx_payload_len_ + this->crc_len_);

  const std::span<const uint8_t> payload(this->rx_payload_, this->rx_payload_len_);
  if (this->tx_gate_mode_ == TX_GATE_FRAME_TRIGGER &&
      frame_has_prefix(payload, this->tx_gate_frame_type_, this->tx_gate_frame_type_len_))
    this->send_next_(now);
  this->frame_callback_.call(payload);
}

bool RS485FrameHub::check_crc_() const {
  return this->crc_len_ == 0 || (this->accept_header_crc_ && this->crc_matches_(true)) ||
         (this->accept_payload_crc_ && this->crc_matches_(false));
}

bool RS485FrameHub::crc_matches_(bool include_header) const {
  uint8_t crc[2]{};
  this->encode_crc_(this->rx_payload_, this->rx_payload_len_, include_header, crc);
  return memcmp(crc, this->rx_payload_ + this->rx_payload_len_, this->crc_len_) == 0;
}

void RS485FrameHub::encode_crc_(const uint8_t *data, size_t len, bool include_header, uint8_t *out) const {
  const uint8_t header[2] = {this->dle_, this->stx_};
  const size_t header_len = include_header ? sizeof(header) : 0;
  uint16_t crc;
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
      crc = sum16_bytes(data, len, sum16_bytes(header, header_len));
      break;
    case CRC_TYPE_CRC16_MODBUS:
      crc = crc16(data, static_cast<uint16_t>(len), crc16(header, static_cast<uint16_t>(header_len)));
      break;
    default:
      return;
  }
  put_le16(out, crc);
}

void RS485FrameHub::maybe_tx_(uint32_t now) {
  if (this->tx_queue_count_ == 0 || this->tx_start_pending_ || this->tx_gate_mode_ == TX_GATE_FRAME_TRIGGER)
    return;
  // Both gates count the interval from the end of our last frame on the wire.
  if (now - this->last_tx_time_ < this->last_tx_wire_ms_ + this->tx_gate_interval_)
    return;
  if (this->tx_gate_mode_ == TX_GATE_FIXED_DELAY || this->bus_silent_(now))
    this->send_next_(now);
}

void RS485FrameHub::send_next_(uint32_t now) {
  if (this->tx_start_pending_ || this->tx_queue_count_ == 0)
    return;
  this->tx_delay_start_ = now;
  this->tx_start_pending_ = true;
}

void RS485FrameHub::transmit_head_(uint32_t now) {
  const uint8_t *frame = this->tx_slot_(this->tx_queue_head_);
  const size_t len = this->tx_queue_len_[this->tx_queue_head_];
  // Escaped into a stack chunk on the way out, so the queue only holds the unescaped bytes.
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
  put(this->dle_);
  put(this->stx_);
  for (size_t i = 0; i < len; i++) {
    put(frame[i]);
    if (frame[i] == this->dle_)
      put(this->escape_marker_);
  }
  put(this->dle_);
  put(this->etx_);
  if (pos > 0)
    this->write_array(chunk, pos);
  wire_len += pos;
#if defined(USE_ESP8266) || defined(USE_RP2)
  // These UARTs do not drive flow_control_pin, so an auto-DE transceiver needs the bytes on the
  // wire before returning; ESP32 drives DE in hardware RS485 mode.
  this->flush();
#endif
  if (this->dump_frames_)
    this->log_frame_(LOG_STR("TX"), frame, len);
  if (++this->tx_queue_head_ == this->max_queue_size_)
    this->tx_queue_head_ = 0;
  this->tx_queue_count_--;
  this->last_tx_time_ = now;
  this->last_tx_wire_ms_ = this->wire_time_ms_(wire_len);
  this->last_activity_time_ = now;
}

uint16_t RS485FrameHub::wire_time_ms_(size_t len) const {
  const uint32_t baud = this->parent_->get_baud_rate();
  if (baud == 0)
    return 0;
  // Start bit, data bits, parity bit and stop bits
  const uint32_t bits = 1 + this->parent_->get_data_bits() +
                        (this->parent_->get_parity() != uart::UART_CONFIG_PARITY_NONE ? 1 : 0) +
                        this->parent_->get_stop_bits();
  // Saturates only at a baud rate far below anything RS485 runs at.
  return std::min<uint32_t>((len * bits * 1000 + baud - 1) / baud, UINT16_MAX);
}

void RS485FrameHub::log_frame_(const LogString *dir, const uint8_t *data, size_t len) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
  // Long frames are logged over several lines to keep the buffer on the stack.
  static constexpr size_t BYTES_PER_LINE = 32;
  char hex[format_hex_size(BYTES_PER_LINE)];
  for (size_t pos = 0; pos < len; pos += BYTES_PER_LINE) {
    format_hex_to(hex, data + pos, std::min(len - pos, BYTES_PER_LINE));
    ESP_LOGD(TAG, "%s %s", LOG_STR_ARG(dir), hex);
  }
#endif
}

}  // namespace esphome::rs485_frame

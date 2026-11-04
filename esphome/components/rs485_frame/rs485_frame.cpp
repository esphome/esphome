#include "rs485_frame.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

#include <cinttypes>

namespace esphome::rs485_frame {

ESPHOME_LOG_TAG(TAG, "rs485_frame");

PROGMEM_STRING_TABLE(CrcTypeStrings, "none", "sum8", "sum16_big_endian", "sum16_little_endian", "xor8", "crc16_modbus");
PROGMEM_STRING_TABLE(CrcVariantStrings, "header_inclusive", "payload_only");
PROGMEM_STRING_TABLE(GateModeStrings, "frame_trigger", "idle_gap", "fixed_delay");
PROGMEM_STRING_TABLE(QueuePolicyStrings, "replace_latest", "fifo");

void RS485FrameHub::setup() {
  this->rx_payload_ = std::make_unique<uint8_t[]>(this->max_frame_length_ + this->crc_length_());
  if (!this->sniffer_only_) {
    this->tx_queue_ = std::make_unique<uint8_t[]>(this->max_queue_size_ * this->tx_slot_capacity_());
    this->tx_queue_len_ = std::make_unique<uint16_t[]>(this->max_queue_size_);
  }
  if (this->dump_frames_)
    this->hex_log_buf_ = std::make_unique<char[]>(format_hex_size(this->tx_slot_capacity_()));
  // idle_gap waits min_silence from boot on a quiet bus; fixed_delay may send right away.
  const uint32_t now = millis();
  this->last_activity_time_ = now;
  this->last_tx_time_ = now - this->tx_fixed_interval_;
}

void RS485FrameHub::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  this->read_available([this, now](uint8_t byte) { this->handle_rx_byte_(byte, now); });

  // A partial frame left by a cable pull or noise burst would otherwise stall reception.
  if (this->in_frame_ && this->in_frame_timeout_ms_ > 0 && now - this->last_rx_time_ >= this->in_frame_timeout_ms_) {
    ESP_LOGW(TAG, "Intra-frame timeout, resetting receive state");
    this->reset_rx_();
  }

  this->maybe_tx_(now);
  if (this->tx_start_pending_ && now - this->tx_delay_start_ >= this->tx_gate_delay_) {
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
                "  TX idle gap: %" PRIu32 "ms, TX interval: %" PRIu32 "ms\n"
                "  Queue policy: %s, queue size: %u\n"
                "  Max frame length: %" PRIu32 ", frame timeout: %" PRIu32 "ms\n"
                "  Sniffer only: %s, dump frames: %s",
                this->dle_, this->stx_, this->etx_, this->escape_marker_,
                LOG_STR_ARG(CrcTypeStrings::get_log_str(this->crc_type_, 0)),
                LOG_STR_ARG(CrcVariantStrings::get_log_str(this->tx_crc_variant_, 0)), YESNO(this->accept_header_crc_),
                YESNO(this->accept_payload_crc_), LOG_STR_ARG(GateModeStrings::get_log_str(this->tx_gate_mode_, 0)),
                gate_hex, this->tx_gate_delay_, this->tx_idle_gap_, this->tx_fixed_interval_,
                LOG_STR_ARG(QueuePolicyStrings::get_log_str(this->queue_policy_, 0)), this->max_queue_size_,
                this->max_frame_length_, this->in_frame_timeout_ms_, YESNO(this->sniffer_only_),
                YESNO(this->dump_frames_));
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
  } else if (len < MIN_FRAME_LENGTH || len > this->max_frame_length_) {
    ESP_LOGW(TAG, "Frame length %zu is outside %zu..max_frame_length (%" PRIu32 "); dropping", len, MIN_FRAME_LENGTH,
             this->max_frame_length_);
  } else {
    return true;
  }
  return false;
}

bool RS485FrameHub::queue_raw_frame(std::span<const uint8_t> frame_type, std::span<const uint8_t> payload) {
  const size_t len = frame_type.size() + payload.size();
  if (!this->check_tx_(len))
    return false;
  size_t index = (this->tx_queue_head_ + this->tx_queue_count_) % this->max_queue_size_;
  if (this->queue_policy_ == QUEUE_REPLACE_LATEST) {
    // The only slot; a frame waiting out the gate delay is replaced and keeps its start time.
    if (this->tx_queue_count_ > 0) {
      ESP_LOGW(TAG, "Replacing pending frame before it was transmitted");
    }
    index = this->tx_queue_head_;
    this->tx_queue_count_ = 0;
  } else if (this->tx_queue_count_ >= this->max_queue_size_) {
    ESP_LOGW(TAG, "Frame queue full; dropping frame");
    return false;
  }
  uint8_t *slot = this->tx_slot_(index);
  // Stage the unescaped bytes at the end of the slot and frame them forward into it.
  uint8_t *staged = slot + this->tx_slot_capacity_() - len;
  progmem_memcpy(staged, frame_type.data(), frame_type.size());
  progmem_memcpy(staged + frame_type.size(), payload.data(), payload.size());
  this->tx_queue_len_[index] = this->build_frame_(staged, len, slot);
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
  if (this->rx_payload_len_ == this->max_frame_length_ + this->crc_length_()) {
    ESP_LOGW(TAG, "Frame exceeded max_frame_length; resetting");
    this->reset_rx_();
    return;
  }
  this->rx_payload_[this->rx_payload_len_++] = byte;
}

void RS485FrameHub::process_frame_(uint32_t now) {
  const size_t crc_len = this->crc_length_();
  if (this->rx_payload_len_ < MIN_FRAME_LENGTH + crc_len)
    return;
  this->rx_payload_len_ -= crc_len;
  if (!this->check_crc_())
    return;
  if (this->dump_frames_)
    this->log_frame_("RX", this->rx_payload_.get(), this->rx_payload_len_ + crc_len);

  const std::span<const uint8_t> payload(this->rx_payload_.get(), this->rx_payload_len_);
  if (this->tx_gate_mode_ == TX_GATE_FRAME_TRIGGER && this->tx_gate_frame_type_len_ > 0 &&
      frame_has_prefix(payload, this->tx_gate_frame_type_, this->tx_gate_frame_type_len_))
    this->send_next_(now);
  this->frame_callback_.call(payload);
}

bool RS485FrameHub::check_crc_() const {
  const size_t crc_len = this->crc_length_();
  if (crc_len == 0)
    return true;
  const uint8_t *payload = this->rx_payload_.get();
  const size_t payload_len = this->rx_payload_len_;
  const uint8_t *crc_bytes = payload + payload_len;
  uint16_t received_crc;
  if (crc_len == 1) {
    received_crc = crc_bytes[0];
  } else if (this->crc_little_endian_()) {
    received_crc = encode_uint16(crc_bytes[1], crc_bytes[0]);
  } else {
    received_crc = encode_uint16(crc_bytes[0], crc_bytes[1]);
  }
  return (this->accept_header_crc_ && this->calculate_crc_(payload, payload_len, true) == received_crc) ||
         (this->accept_payload_crc_ && this->calculate_crc_(payload, payload_len, false) == received_crc);
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

size_t RS485FrameHub::build_frame_(const uint8_t *payload, size_t len, uint8_t *out) const {
  // The CRC covers the unescaped payload, so take it before the frame overwrites a staged payload.
  const uint16_t crc = this->calculate_crc_(payload, len, this->tx_crc_variant_ == CRC_HEADER_INCLUSIVE);
  size_t pos = 0;
  auto append_escaped = [this, out, &pos](uint8_t byte) {
    out[pos++] = byte;
    if (byte == this->dle_)
      out[pos++] = this->escape_marker_;
  };
  out[pos++] = this->dle_;
  out[pos++] = this->stx_;
  for (size_t i = 0; i < len; i++)
    append_escaped(payload[i]);
  // The CRC bytes are escaped like payload bytes.
  const size_t crc_len = this->crc_length_();
  if (crc_len == 1) {
    append_escaped(crc & 0xFF);
  } else if (crc_len == 2 && this->crc_little_endian_()) {
    append_escaped(crc & 0xFF);
    append_escaped(crc >> 8);
  } else if (crc_len == 2) {
    append_escaped(crc >> 8);
    append_escaped(crc & 0xFF);
  }
  out[pos++] = this->dle_;
  out[pos++] = this->etx_;
  return pos;
}

void RS485FrameHub::maybe_tx_(uint32_t now) {
  if (this->tx_gate_mode_ == TX_GATE_IDLE_GAP && now - this->last_activity_time_ >= this->tx_idle_gap_)
    this->send_next_(now);
  if (this->tx_gate_mode_ == TX_GATE_FIXED_DELAY && now - this->last_tx_time_ >= this->tx_fixed_interval_)
    this->send_next_(now);
}

void RS485FrameHub::send_next_(uint32_t now) {
  if (this->sniffer_only_ || this->tx_start_pending_ || this->tx_queue_count_ == 0)
    return;
  if (this->tx_gate_delay_ > 0) {
    this->tx_delay_start_ = now;
    this->tx_start_pending_ = true;
  } else {
    this->transmit_head_(now);
  }
}

void RS485FrameHub::transmit_head_(uint32_t now) {
  const uint8_t *frame = this->tx_slot_(this->tx_queue_head_);
  const size_t len = this->tx_queue_len_[this->tx_queue_head_];
  this->write_array(frame, len);
#if defined(USE_ESP8266) || defined(USE_RP2)
  // These UARTs do not drive flow_control_pin, so an auto-DE transceiver needs the bytes on the
  // wire before returning; ESP32 drives DE in hardware RS485 mode.
  this->flush();
#endif
  if (this->dump_frames_)
    this->log_frame_("TX", frame, len);
  this->tx_queue_head_ = (this->tx_queue_head_ + 1) % this->max_queue_size_;
  this->tx_queue_count_--;
  this->last_tx_time_ = now;
  this->last_activity_time_ = now;
}

void RS485FrameHub::log_frame_(const char *dir, const uint8_t *data, size_t len) {
  format_hex_to(this->hex_log_buf_.get(), format_hex_size(this->tx_slot_capacity_()), data, len);
  ESP_LOGD(TAG, "%s %s", dir, this->hex_log_buf_.get());
}

}  // namespace esphome::rs485_frame

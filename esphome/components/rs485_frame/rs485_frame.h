#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"

#include <memory>
#include <span>

namespace esphome::rs485_frame {

// Must agree with the cv.Length(max=8) frame_type cap in __init__.py.
static constexpr size_t MAX_FRAME_TYPE_LEN = 8;
// Framing overhead added to every TX frame: DLE+STX(2) + DLE+ETX(2) + escaped CRC max(4).
static constexpr size_t FRAME_OVERHEAD_BYTES = 8;

enum CrcVariant : uint8_t {
  CRC_HEADER_INCLUSIVE,
  CRC_PAYLOAD_ONLY,
};

enum CrcType : uint8_t {
  CRC_TYPE_NONE,
  CRC_TYPE_SUM8,
  CRC_TYPE_SUM16_BIG_ENDIAN,
  CRC_TYPE_SUM16_LITTLE_ENDIAN,
  CRC_TYPE_XOR8,
  CRC_TYPE_CRC16_MODBUS,
};

enum QueuePolicy : uint8_t {
  QUEUE_REPLACE_LATEST,
  QUEUE_FIFO,
};

enum TxGateMode : uint8_t {
  TX_GATE_FRAME_TRIGGER,
  TX_GATE_IDLE_GAP,
  TX_GATE_FIXED_DELAY,
};

/// True when payload starts with the len-byte prefix, which may live in PROGMEM.
inline bool frame_has_prefix(std::span<const uint8_t> payload, const uint8_t *prefix, size_t len) {
  if (payload.size() < len)
    return false;
  for (size_t i = 0; i < len; i++) {
    if (payload[i] != progmem_read_byte(prefix + i))
      return false;
  }
  return true;
}

/// Hub for a DLE-framed RS485 bus: RX framer, CRC engine, gated TX queue and on_frame callbacks.
class RS485FrameHub : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_framing(uint8_t dle, uint8_t stx, uint8_t etx, uint8_t escape_marker);
  void set_accept_header_crc(bool accept) { this->accept_header_crc_ = accept; }
  void set_accept_payload_crc(bool accept) { this->accept_payload_crc_ = accept; }
  void set_crc_type(CrcType type) { this->crc_type_ = type; }
  void set_tx_crc_variant(CrcVariant variant) { this->tx_crc_variant_ = variant; }
  void set_tx_gate_mode(TxGateMode mode) { this->tx_gate_mode_ = mode; }
  /// frame_type may live in PROGMEM.
  void set_tx_gate_frame_type(const uint8_t *frame_type, size_t len) {
    this->tx_gate_frame_type_ = frame_type;
    this->tx_gate_frame_type_len_ = len;
  }
  void set_tx_gate_delay(uint32_t delay) { this->tx_gate_delay_ = delay; }
  void set_tx_idle_gap(uint32_t idle_gap) { this->tx_idle_gap_ = idle_gap; }
  void set_tx_fixed_interval(uint32_t interval) { this->tx_fixed_interval_ = interval; }
  void set_queue_policy(QueuePolicy policy) { this->queue_policy_ = policy; }
  void set_max_queue_size(uint8_t size) { this->max_queue_size_ = size; }
  void set_dump_frames(bool dump_frames) { this->dump_frames_ = dump_frames; }
  void set_sniffer_only(bool sniffer_only) { this->sniffer_only_ = sniffer_only; }
  void set_max_frame_length(uint32_t length) { this->max_frame_length_ = length; }
  void set_in_frame_timeout(uint32_t ms) { this->in_frame_timeout_ms_ = ms; }

  /// Frame and queue an unframed payload; data may live in PROGMEM.
  bool queue_frame(const uint8_t *data, size_t len) { return this->queue_raw_frame({data, len}, {}); }
  /// Frame and queue frame_type followed by payload; both may live in PROGMEM.
  bool queue_raw_frame(std::span<const uint8_t> frame_type, std::span<const uint8_t> payload);
  /// Called with the unescaped payload (frame_type first, CRC removed) of every valid frame.
  template<typename F> void add_on_frame_callback(F &&callback) {
    this->frame_callback_.add(std::forward<F>(callback));
  }

 protected:
  void handle_rx_byte_(uint8_t byte, uint32_t now);
  void process_raw_frame_(uint32_t now);
  void reset_rx_();
  bool validate_frame_();
  uint16_t calculate_crc_(const uint8_t *data, size_t len, bool include_header) const;
  size_t crc_length_() const;
  bool crc_little_endian_() const {
    return this->crc_type_ == CRC_TYPE_CRC16_MODBUS || this->crc_type_ == CRC_TYPE_SUM16_LITTLE_ENDIAN;
  }
  size_t tx_slot_capacity_() const { return this->max_frame_length_ * 2 + FRAME_OVERHEAD_BYTES; }
  uint8_t *tx_slot_(size_t index) const { return this->tx_queue_.get() + index * this->tx_slot_capacity_(); }
  /// Writes the framed payload to out and returns its length. payload may sit at the end of
  /// out's tx slot, since the escaped frame is written forward and never overtakes it.
  size_t build_frame_(const uint8_t *payload, size_t len, uint8_t *out) const;
  bool check_tx_(size_t len);
  void maybe_tx_(uint32_t now);
  void send_next_(uint32_t now);
  void transmit_head_(uint32_t now);
  void log_frame_(const char *dir, const uint8_t *data, size_t len);

  // Empty for idle_gap / fixed_delay / sniffer hubs, so the gate matcher never fires.
  const uint8_t *tx_gate_frame_type_{nullptr};
  uint32_t tx_gate_delay_{0};
  uint32_t tx_idle_gap_{4};
  uint32_t tx_fixed_interval_{100};
  uint32_t max_frame_length_{128};
  uint32_t in_frame_timeout_ms_{50};

  LazyCallbackManager<void(std::span<const uint8_t>)> frame_callback_;
  // Ring of max_queue_size_ slots of tx_slot_capacity_() bytes each, framed in place. While
  // tx_start_pending_ the head slot is the frame waiting out the gate delay.
  std::unique_ptr<uint8_t[]> tx_queue_;
  std::unique_ptr<uint16_t[]> tx_queue_len_;
  std::unique_ptr<uint8_t[]> raw_frame_;
  // The `payload` on_frame receives.
  std::unique_ptr<uint8_t[]> rx_payload_;
  // Only allocated with dump_frames.
  std::unique_ptr<char[]> hex_log_buf_;
  uint32_t last_rx_time_{0};
  uint32_t last_tx_time_{0};
  // Advanced by RX and our own TX.
  uint32_t last_activity_time_{0};
  uint32_t tx_start_at_{0};
  uint16_t raw_len_{0};
  uint16_t rx_payload_len_{0};

  uint8_t dle_{0x10};
  uint8_t stx_{0x02};
  uint8_t etx_{0x03};
  // Equals dle_ in doubling mode (a literal DLE is sent as DLE DLE).
  uint8_t escape_marker_{0x00};
  CrcType crc_type_{CRC_TYPE_SUM16_BIG_ENDIAN};
  CrcVariant tx_crc_variant_{CRC_HEADER_INCLUSIVE};
  TxGateMode tx_gate_mode_{TX_GATE_FRAME_TRIGGER};
  QueuePolicy queue_policy_{QUEUE_REPLACE_LATEST};
  uint8_t tx_gate_frame_type_len_{0};
  uint8_t max_queue_size_{1};
  uint8_t tx_queue_head_{0};
  uint8_t tx_queue_count_{0};
  uint8_t previous_byte_{0};
  bool accept_header_crc_{true};
  bool accept_payload_crc_{true};
  bool dump_frames_{false};
  bool sniffer_only_{false};
  bool in_frame_{false};
  // An in-frame DLE whose meaning the next byte decides (needed for doubling mode).
  bool after_dle_{false};
  bool has_activity_{false};
  bool has_tx_ever_{false};
  bool tx_start_pending_{false};
};

}  // namespace esphome::rs485_frame

#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"

#include <memory>
#include <vector>

namespace esphome::rs485_frame {

// Must agree with the cv.Length(max=8) frame_type cap in __init__.py.
static constexpr size_t MAX_FRAME_TYPE_LEN = 8;
// Framing overhead added to every TX frame: DLE+STX(2) + DLE+ETX(2) + escaped CRC max(4).
static constexpr size_t FRAME_OVERHEAD_BYTES = 8;

enum CrcVariant {
  CRC_HEADER_INCLUSIVE,
  CRC_PAYLOAD_ONLY,
};

enum CrcType {
  CRC_TYPE_NONE,
  CRC_TYPE_SUM8,
  CRC_TYPE_SUM16_BIG_ENDIAN,
  CRC_TYPE_SUM16_LITTLE_ENDIAN,
  CRC_TYPE_XOR8,
  CRC_TYPE_CRC16_MODBUS,
};

enum QueuePolicy {
  QUEUE_REPLACE_LATEST,
  QUEUE_FIFO,
};

enum TxGateMode {
  TX_GATE_FRAME_TRIGGER,
  TX_GATE_IDLE_GAP,
  TX_GATE_FIXED_DELAY,
};

/// True when payload starts with the len-byte prefix, which may live in PROGMEM.
inline bool frame_has_prefix(const std::vector<uint8_t> &payload, const uint8_t *prefix, size_t len) {
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
  void set_tx_gate_frame_type(const std::vector<uint8_t> &frame_type) {
    this->tx_gate_frame_type_.assign(frame_type.begin(), frame_type.end());
  }
  void set_tx_gate_delay(uint32_t delay) { this->tx_gate_delay_ = delay; }
  void set_tx_idle_gap(uint32_t idle_gap) { this->tx_idle_gap_ = idle_gap; }
  void set_tx_fixed_interval(uint32_t interval) { this->tx_fixed_interval_ = interval; }
  void set_queue_policy(QueuePolicy policy) { this->queue_policy_ = policy; }
  void set_max_queue_size(uint32_t size) { this->max_queue_size_ = size; }
  void set_dump_frames(bool dump_frames) { this->dump_frames_ = dump_frames; }
  void set_sniffer_only(bool sniffer_only) { this->sniffer_only_ = sniffer_only; }
  void set_max_frame_length(uint32_t length) { this->max_frame_length_ = length; }
  void set_in_frame_timeout(uint32_t ms) { this->in_frame_timeout_ms_ = ms; }

  /// Frame and queue an unframed payload; data may live in PROGMEM.
  bool queue_frame(const uint8_t *data, size_t len);
  bool queue_raw_frame(const std::vector<uint8_t> &frame_type, const std::vector<uint8_t> &payload);
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
  void append_escaped_(uint8_t byte, std::vector<uint8_t> &out) const;
  void build_frame_(const std::vector<uint8_t> &payload, std::vector<uint8_t> &out);
  bool check_tx_(size_t len);
  bool enqueue_frame_();
  void maybe_tx_(uint32_t now);
  void send_next_(uint32_t now);
  void transmit_(std::vector<uint8_t> &frame, uint32_t now);
  void queue_pop_front_();
  void write_frame_(const std::vector<uint8_t> &frame);
  void log_frame_(const char *dir, const uint8_t *data, size_t len);

  uint8_t dle_{0x10};
  uint8_t stx_{0x02};
  uint8_t etx_{0x03};
  // Equals dle_ in doubling mode (a literal DLE is sent as DLE DLE).
  uint8_t escape_marker_{0x00};
  bool accept_header_crc_{true};
  bool accept_payload_crc_{true};
  CrcType crc_type_{CRC_TYPE_SUM16_BIG_ENDIAN};
  CrcVariant tx_crc_variant_{CRC_HEADER_INCLUSIVE};
  TxGateMode tx_gate_mode_{TX_GATE_FRAME_TRIGGER};
  // Empty for idle_gap / fixed_delay / sniffer hubs, so the gate matcher never fires.
  StaticVector<uint8_t, MAX_FRAME_TYPE_LEN> tx_gate_frame_type_;
  uint32_t tx_gate_delay_{0};
  uint32_t tx_idle_gap_{4};
  uint32_t tx_fixed_interval_{100};
  QueuePolicy queue_policy_{QUEUE_REPLACE_LATEST};
  uint32_t max_queue_size_{1};
  bool dump_frames_{false};
  bool sniffer_only_{false};
  uint32_t max_frame_length_{128};
  uint32_t in_frame_timeout_ms_{50};

  LazyCallbackManager<void(const std::vector<uint8_t> &)> frame_callback_;
  // Ring buffer; every slot is reserved in setup() and frames are swapped in and out, never copied.
  std::vector<std::vector<uint8_t>> tx_queue_;
  size_t tx_queue_head_{0};
  size_t tx_queue_tail_{0};
  size_t tx_queue_count_{0};

  bool in_frame_{false};
  uint8_t previous_byte_{0};
  // An in-frame DLE whose meaning the next byte decides (needed for doubling mode).
  bool after_dle_{false};
  std::unique_ptr<uint8_t[]> raw_frame_;
  size_t raw_len_{0};
  uint32_t last_rx_time_{0};
  uint32_t last_tx_time_{0};
  // Advanced by RX and our own TX.
  uint32_t last_activity_time_{0};
  bool has_activity_{false};
  bool has_tx_ever_{false};
  bool tx_start_pending_{false};
  uint32_t tx_start_at_{0};
  std::vector<uint8_t> pending_tx_frame_;

  // Reserved to their worst case in setup(); rx_payload_ is the `payload` on_frame receives.
  std::vector<uint8_t> rx_payload_;
  std::vector<uint8_t> tx_payload_buf_;
  std::vector<uint8_t> tx_frame_buf_;
  std::unique_ptr<char[]> hex_log_buf_;
  size_t hex_log_buf_size_{0};
};

}  // namespace esphome::rs485_frame

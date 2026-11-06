#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"

#include <span>

namespace esphome::rs485_frame {

// Must agree with MAX_FRAME_TYPE_LEN in __init__.py, which caps frame_type.
static constexpr size_t MAX_FRAME_TYPE_LEN = 8;
// Must agree with MIN_FRAME_LENGTH in __init__.py.
static constexpr size_t MIN_FRAME_LENGTH = 2;
static constexpr uint8_t NO_SLOT = 0xFF;

enum CrcVariant : uint8_t { CRC_HEADER_INCLUSIVE, CRC_PAYLOAD_ONLY };
enum CrcType : uint8_t {
  CRC_TYPE_NONE,
  CRC_TYPE_SUM8,
  CRC_TYPE_SUM16_BIG_ENDIAN,
  CRC_TYPE_SUM16_LITTLE_ENDIAN,
  CRC_TYPE_XOR8,
  CRC_TYPE_CRC16_MODBUS,
};
constexpr uint8_t crc_length(CrcType type) {
  return type == CRC_TYPE_NONE ? 0 : (type == CRC_TYPE_SUM8 || type == CRC_TYPE_XOR8) ? 1 : 2;
}
enum QueuePolicy : uint8_t { QUEUE_REPLACE_LATEST, QUEUE_FIFO };
enum TxGateMode : uint8_t { TX_GATE_FRAME_TRIGGER, TX_GATE_IDLE_GAP, TX_GATE_FIXED_DELAY };
/// The _DLE states mean the last byte was a DLE.
enum RxState : uint8_t { RX_STATE_IDLE, RX_STATE_IDLE_DLE, RX_STATE_FRAME, RX_STATE_FRAME_DLE };

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
  /// Buffers are sized by codegen; tx_queue and tx_queue_len are nullptr on a sniffer_only hub.
  RS485FrameHub(uint8_t dle, uint8_t stx, uint8_t etx, uint8_t escape_marker, CrcType crc_type,
                uint16_t max_frame_length, uint8_t max_queue_size, uint8_t *rx_payload, uint8_t *tx_queue,
                uint16_t *tx_queue_len)
      : rx_payload_(rx_payload),
        tx_queue_(tx_queue),
        tx_queue_len_(tx_queue_len),
        max_frame_length_(max_frame_length),
        dle_(dle),
        stx_(stx),
        etx_(etx),
        escape_marker_(escape_marker),
        crc_type_(crc_type),
        crc_len_(crc_length(crc_type)),
        max_queue_size_(max_queue_size) {}

  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_accept_header_crc(bool accept) { this->flags_.accept_header_crc = accept; }
  void set_accept_payload_crc(bool accept) { this->flags_.accept_payload_crc = accept; }
  void set_tx_crc_variant(CrcVariant variant) { this->tx_crc_variant_ = variant; }
  void set_tx_gate_mode(TxGateMode mode) { this->tx_gate_mode_ = mode; }
  void set_tx_gate_frame_type(const uint8_t *frame_type, size_t len) {
    this->tx_gate_frame_type_ = frame_type;
    this->tx_gate_frame_type_len_ = len;
  }
  void set_tx_gate_delay(uint16_t delay) { this->tx_gate_delay_ = delay; }
  void set_tx_gate_interval(uint16_t interval) { this->tx_gate_interval_ = interval; }
  void set_queue_policy(QueuePolicy policy) { this->queue_policy_ = policy; }
  void set_dump_frames(bool dump_frames) { this->flags_.dump_frames = dump_frames; }
  void set_in_frame_timeout(uint16_t ms) { this->in_frame_timeout_ms_ = ms; }

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
  void append_rx_(uint8_t byte);
  void process_frame_(uint32_t now);
  void reset_rx_();
  bool check_crc_() const;
  bool crc_matches_(bool include_header) const;
  void encode_crc_(const uint8_t *data, size_t len, bool include_header, uint8_t *out) const;
  uint8_t *tx_slot_(size_t index) const { return this->tx_queue_ + index * (this->max_frame_length_ + this->crc_len_); }
  void maybe_tx_(uint32_t now);
  bool bus_silent_(uint32_t now) const { return now - this->last_activity_time_ >= this->tx_gate_interval_; }
  void send_next_(uint32_t now);
  void transmit_head_(uint32_t now);
  /// Passes the wire bytes of an unescaped frame to put: DLE STX, the escaped frame, DLE ETX.
  template<typename F> void encode_wire_(const uint8_t *data, size_t len, F &&put) const {
    put(this->dle_);
    put(this->stx_);
    for (size_t i = 0; i < len; i++) {
      put(data[i]);
      if (data[i] == this->dle_)
        put(this->escape_marker_);
    }
    put(this->dle_);
    put(this->etx_);
  }
  /// Whether the released frame may go out once its gate delay has passed.
  bool gate_allows_send_(uint32_t now) {
    return this->tx_gate_mode_ == TX_GATE_FIXED_DELAY ||
           ((this->tx_gate_mode_ != TX_GATE_IDLE_GAP || this->bus_silent_(now)) && this->available() == 0);
  }
  uint16_t wire_time_ms_(size_t len) const;
  void log_frame_(const LogString *dir, const uint8_t *data, size_t len);

  uint8_t *const rx_payload_;
  // Ring of unescaped frames plus CRC; while tx_start_pending the head is waiting out the gate delay.
  uint8_t *const tx_queue_;
  uint16_t *const tx_queue_len_;
  const uint8_t *tx_gate_frame_type_{nullptr};
  LazyCallbackManager<void(std::span<const uint8_t>)> frame_callback_;
  uint32_t last_rx_time_{0};
  uint32_t last_tx_time_{0};
  uint32_t last_activity_time_{0};  // RX or our own TX, seeded in setup()
  uint32_t tx_delay_start_{0};      // When the frame waiting out the gate delay was released
  uint16_t tx_gate_delay_{0};
  uint16_t tx_gate_interval_{0};
  uint16_t in_frame_timeout_ms_{50};
  uint16_t max_frame_length_;
  uint16_t rx_payload_len_{0};
  uint16_t last_tx_wire_ms_{0};  // The gates count from the end of the last frame on the wire

  uint8_t dle_;
  uint8_t stx_;
  uint8_t etx_;
  uint8_t escape_marker_;  // Equals dle_ in doubling mode
  CrcType crc_type_;
  uint8_t crc_len_;
  CrcVariant tx_crc_variant_{CRC_HEADER_INCLUSIVE};
  TxGateMode tx_gate_mode_{TX_GATE_FRAME_TRIGGER};
  QueuePolicy queue_policy_{QUEUE_REPLACE_LATEST};
  uint8_t tx_gate_frame_type_len_{0};
  uint8_t max_queue_size_;
  uint8_t tx_queue_head_{0};
  uint8_t tx_queue_count_{0};
  uint8_t tx_sending_slot_{NO_SLOT};  // queue_raw_frame() must not overwrite it
  RxState rx_state_{RX_STATE_IDLE};
  struct Flags {
    uint8_t accept_header_crc : 1;
    uint8_t accept_payload_crc : 1;
    uint8_t dump_frames : 1;
    uint8_t tx_start_pending : 1;
    uint8_t reserved : 4;
  } flags_{.accept_header_crc = true, .accept_payload_crc = true};
};

}  // namespace esphome::rs485_frame

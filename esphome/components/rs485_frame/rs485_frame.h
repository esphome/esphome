#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/uart/uart.h"

#include <memory>
#include <vector>

namespace esphome::rs485_frame {

// Maximum number of bytes in a frame-type prefix (schema cap: cv.Length(max=8)).
// StaticVector template parameters and the Python cv.Length(max=) validator must agree.
static constexpr size_t MAX_FRAME_TYPE_LEN = 8;

// Maximum number of alternative frame-type prefixes a single on_frame: trigger can match.
// on_frame.frame_type accepts either a single prefix (e.g. [0x01, 0x03]) or a list of
// prefixes (e.g. [[0x01, 0x03], [0x01, 0x09]]) so one lambda can decode multiple related
// frame types. Cap exists to bound the trigger's static storage; the Python schema rejects
// any list longer than this. Bump both sides together if more alternates are ever needed.
static constexpr size_t MAX_FRAME_TYPE_ALTS = 4;

// Framing overhead added to every TX frame: DLE+STX(2) + DLE+ETX(2) + escaped CRC max(4).
static constexpr size_t FRAME_OVERHEAD_BYTES = 8;

// Maximum preamble / postamble byte-list lengths for command_format. Must agree with the
// Python schema caps (cv.Length(max=8)) so the StaticVectors are never over-filled.
static constexpr size_t MAX_COMMAND_PREAMBLE_LEN = 8;
static constexpr size_t MAX_COMMAND_POSTAMBLE_LEN = 8;

// Maximum number of command values a single `command:` button entry may carry, serialised
// back-to-back in one frame; must agree with the Python schema cap (cv.Length(max=...)).
static constexpr size_t MAX_COMMAND_VALUES = 8;

/// Diagnostic value exposed by the rs485_frame sensor/text_sensor platforms.
/// These are hub state, not user payload decoding — user decoding is done via on_frame:.
enum SensorDecode {
  SENSOR_DECODE_FRAMES_RECEIVED,    ///< Running count of validated RX frames.
  SENSOR_DECODE_CRC_FAILURES,       ///< Running count of frames that failed validation (CRC or structural).
  SENSOR_DECODE_COMMANDS_SENT,      ///< Running count of transmitted user commands.
  SENSOR_DECODE_COMMAND_DROPS,      ///< Commands dropped (queue full or sniffer mode).
  SENSOR_DECODE_LAST_KEEPALIVE_MS,  ///< Interval (ms) between the last two gate frames.
  SENSOR_DECODE_QUEUE_DEPTH,        ///< Current TX queue depth.
  // A discarded frame never reached validate_frame_() at all (it was abandoned mid-receive),
  // so it is not a CRC/structural failure -- kept as its own counter rather than folded into
  // crc_failures.
  SENSOR_DECODE_DISCARDED_FRAMES,  ///< RX frames abandoned mid-receive (overflow or intra-frame timeout).
};

/// Which bytes are included in the CRC calculation.
enum CrcVariant {
  CRC_HEADER_INCLUSIVE,  ///< DLE+STX preamble bytes are included in the CRC sum (Hayward wireless).
  CRC_PAYLOAD_ONLY,      ///< CRC covers only the unescaped payload bytes (Hayward wired remotes).
};

/// CRC algorithm applied to each frame.
enum CrcType {
  CRC_TYPE_NONE,                 ///< No CRC — every structurally valid frame is accepted.
  CRC_TYPE_SUM8,                 ///< 8-bit arithmetic sum.
  CRC_TYPE_SUM16_BIG_ENDIAN,     ///< 16-bit arithmetic sum, CRC bytes on the wire high byte first.
  CRC_TYPE_SUM16_LITTLE_ENDIAN,  ///< 16-bit arithmetic sum, CRC bytes on the wire low byte first.
  CRC_TYPE_XOR8,                 ///< 8-bit XOR.
  CRC_TYPE_CRC16_MODBUS,         ///< CRC-16/MODBUS (poly 0xA001, init 0xFFFF, little-endian output).
};

/// TX queue overflow strategy.
enum QueuePolicy {
  QUEUE_REPLACE_LATEST,  ///< New command replaces the pending command (max_queue_size must be 1).
  QUEUE_FIFO,            ///< Commands are transmitted in arrival order.
};

/// TX gate trigger mode — controls when queued commands are transmitted.
enum TxGateMode {
  TX_GATE_FRAME_TRIGGER,  ///< Transmit after receiving a specific gate frame type.
  TX_GATE_IDLE_GAP,       ///< Transmit after the bus has been silent for min_silence.
  TX_GATE_FIXED_DELAY,    ///< Transmit on a fixed periodic interval.
};

class RS485FrameHub;

/// Automation trigger fired by the hub when a frame matching one of the configured
/// frame_type prefixes is received. The full decoded payload is passed as the automation
/// argument `payload`.
///
/// **Offset convention: payload-relative.** `payload[0..N-1]` are the N-byte frame_type
/// prefix (typically 2 bytes); data starts at `payload[N]`. DLE+STX, escape bytes, and
/// CRC are already stripped by validate_frame_() — the lambda sees only the unescaped
/// frame contents between (but excluding) the framing delimiters.
///
/// Community references may strip the frame_type before counting (so their "byte 0" is
/// our `payload[2]`). See the rs485_frame docs' "Offset convention" section for the
/// translation table when porting offsets from external research.
///
/// A trigger holds up to MAX_FRAME_TYPE_ALTS frame-type prefixes (StaticVector to avoid
/// heap allocation) and matches if any one of them is a prefix of the payload. An empty
/// frame_types_ matches every frame (the documented `frame_type: []` form). The trigger
/// is registered with the hub via register_trigger().
///
/// Note: build_callback_automation() (preferred by CLAUDE.md for stateless triggers)
/// cannot be used because the trigger must carry its own frame_type filter for the hub
/// to dispatch against. A full Trigger subclass is justified.
class RS485FrameTrigger : public Trigger<const std::vector<uint8_t> &> {
 public:
  // Appends one prefix to this trigger's match list. Called once per prefix from to_code;
  // the schema rejects more than MAX_FRAME_TYPE_ALTS prefixes, so the StaticVector cap is
  // never reached at runtime, but push_back silently drops extras as a final safety net.
  void add_frame_type(const std::vector<uint8_t> &frame_type) {
    StaticVector<uint8_t, MAX_FRAME_TYPE_LEN> prefix;
    prefix.assign(frame_type.begin(), frame_type.end());
    this->frame_types_.push_back(prefix);
  }
  bool matches(const std::vector<uint8_t> &payload) const;

 protected:
  StaticVector<StaticVector<uint8_t, MAX_FRAME_TYPE_LEN>, MAX_FRAME_TYPE_ALTS> frame_types_;
};

/// Central hub for a DLE-framed RS485 bus. Owns the UART framer, TX queue,
/// CRC engine, and the on_frame: trigger registry. User payload decoding is done
/// via on_frame: triggers and globals/template sensors — this class no longer
/// dispatches to per-platform listener subclasses.
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
  // Configure the generic command encoder. Preamble bytes are written first, then each value
  // serialised as value_element_bytes (1/2/4) in the requested byte order, back-to-back,
  // then the postamble bytes.
  void set_command_format(const std::vector<uint8_t> &preamble, uint8_t value_element_bytes, bool big_endian,
                          const std::vector<uint8_t> &postamble) {
    this->cmd_preamble_.assign(preamble.begin(), preamble.end());
    this->cmd_postamble_.assign(postamble.begin(), postamble.end());
    this->cmd_value_element_bytes_ = value_element_bytes;
    this->cmd_big_endian_ = big_endian;
    this->has_command_format_ = true;
  }
  void set_idle_command(uint32_t cmd) {
    this->idle_command_ = cmd;
    this->has_idle_command_ = true;
  }
  void set_dump_frames(bool dump_frames) { this->dump_frames_ = dump_frames; }
  void set_sniffer_only(bool sniffer_only) { this->sniffer_only_ = sniffer_only; }
  void set_max_frame_length(uint32_t length) { this->max_frame_length_ = length; }
  void set_in_frame_timeout(uint32_t ms) { this->in_frame_timeout_ms_ = ms; }

  bool queue_command_value(uint32_t command) { return this->queue_command_values(&command, 1); }
  // Queue one or more values encoded back-to-back using this hub's command_format. Fails
  // (bumps command_drops_, returns false) if the hub has no command_format: -- without one
  // there is no configured preamble/endian/postamble, and silently falling back to a 4-byte
  // big-endian encoding would put the wrong bytes on the wire.
  bool queue_command_values(const uint32_t *commands, size_t count);
  // Queue values using this hub's preamble/endian/postamble but a per-call element byte width.
  // Used by buttons with a top-level value_element_bytes: override (not a full command_format).
  // Fails the same way as queue_command_values() if the hub has no command_format:.
  bool queue_command_values_with_element_bytes(const uint32_t *commands, size_t count, uint8_t element_bytes);
  // preamble/postamble are taken as pointer/length pairs, not std::vector<uint8_t>, so a
  // caller backed by a StaticVector (the button's per-button command_format override) can pass
  // its own storage straight through with no per-call heap allocation.
  bool queue_command_with_format(const uint32_t *commands, size_t count, const uint8_t *preamble, size_t preamble_len,
                                 uint8_t value_element_bytes, bool big_endian, const uint8_t *postamble,
                                 size_t postamble_len);
  bool queue_raw_frame(const std::vector<uint8_t> &payload);
  // Assemble frame_type + payload into a pre-reserved buffer and queue it. Used by the
  // send_frame action and the raw-form button so neither allocates a per-call vector.
  bool queue_raw_frame(const std::vector<uint8_t> &frame_type, const std::vector<uint8_t> &payload);
  void register_trigger(RS485FrameTrigger *trigger) { this->triggers_.push_back(trigger); }

  uint32_t get_frames_received() const { return this->frames_received_; }
  uint32_t get_crc_failures() const { return this->crc_failures_; }
  uint32_t get_commands_sent() const { return this->commands_sent_; }
  uint32_t get_command_drops() const { return this->command_drops_; }
  uint32_t get_discarded_frames() const { return this->discarded_frames_; }
  uint32_t get_last_keepalive_ms() const { return this->last_keepalive_ms_; }
  uint32_t get_queue_depth() const { return this->tx_queue_count_ + (this->tx_start_pending_ ? 1 : 0); }
  const char *get_last_frame_type() const { return this->last_frame_type_; }

 protected:
  void read_uart_(uint32_t now);
  void handle_rx_byte_(uint8_t byte, uint32_t now);
  void process_raw_frame_(uint32_t now);
  // Counts a frame abandoned mid-receive (never reaches validate_frame_()).
  void record_discard_() { this->discarded_frames_++; }
  bool validate_frame_();
  uint16_t calculate_crc_(const std::vector<uint8_t> &payload, bool include_header) const;
  size_t crc_length_() const;
  /// True when the 2-byte CRC is read/emitted low byte first (CRC-16/MODBUS, sum16 little-endian).
  bool crc_little_endian_() const {
    return this->crc_type_ == CRC_TYPE_CRC16_MODBUS || this->crc_type_ == CRC_TYPE_SUM16_LITTLE_ENDIAN;
  }
  void escape_dle_(const std::vector<uint8_t> &data, std::vector<uint8_t> &out) const;
  void build_frame_(const std::vector<uint8_t> &payload, std::vector<uint8_t> &out);
  void build_key_payload_(const uint32_t *commands, size_t count, uint8_t element_bytes,
                          std::vector<uint8_t> &out) const;
  bool enqueue_frame_();
  void maybe_tx_(uint32_t now);
  void send_next_(uint32_t now);
  void send_next_idle_(uint32_t now);
  bool frame_type_equals_(const std::vector<uint8_t> &payload,
                          const StaticVector<uint8_t, MAX_FRAME_TYPE_LEN> &frame_type) const;
  void update_last_frame_type_();
  size_t queue_size_() const { return this->tx_queue_count_; }
  void queue_pop_front_();
  void write_frame_(const std::vector<uint8_t> &frame);
  // Record a transmission: advances last_tx_time_ and the bus-activity timestamp, and
  // latches has_tx_ever_ so the fixed_delay gate has an unambiguous "has transmitted" flag.
  void mark_transmitted_(uint32_t now) {
    this->last_tx_time_ = now;
    this->last_activity_time_ = now;
    this->has_activity_ = true;
    this->has_tx_ever_ = true;
  }

  uint8_t dle_{0x10};
  uint8_t stx_{0x02};
  uint8_t etx_{0x03};
  uint8_t escape_marker_{0x00};
  bool accept_header_crc_{true};
  bool accept_payload_crc_{true};
  CrcType crc_type_{CRC_TYPE_SUM16_BIG_ENDIAN};
  CrcVariant tx_crc_variant_{CRC_HEADER_INCLUSIVE};
  TxGateMode tx_gate_mode_{TX_GATE_FRAME_TRIGGER};
  // Empty by default: there is no protocol-agnostic gate frame. The Python schema requires
  // tx.gate.frame_type for frame_trigger mode, so this is only empty for idle_gap /
  // fixed_delay / sniffer hubs, where the frame matcher must never fire.
  StaticVector<uint8_t, MAX_FRAME_TYPE_LEN> tx_gate_frame_type_;
  uint32_t tx_gate_delay_{0};
  uint32_t tx_idle_gap_{4};
  uint32_t tx_fixed_interval_{100};
  QueuePolicy queue_policy_{QUEUE_REPLACE_LATEST};
  uint32_t max_queue_size_{1};
  // Generic command encoder. has_command_format_ is false for hubs without command_format:
  // (generic hubs with no explicit block). build_key_payload_ is never called in that case
  // — the button platform's _final_validate rejects `value:` against such hubs.
  StaticVector<uint8_t, MAX_COMMAND_PREAMBLE_LEN> cmd_preamble_;
  StaticVector<uint8_t, MAX_COMMAND_POSTAMBLE_LEN> cmd_postamble_;
  uint8_t cmd_value_element_bytes_{4};
  bool cmd_big_endian_{true};
  bool has_command_format_{false};
  uint32_t idle_command_{0};
  bool has_idle_command_{false};
  bool dump_frames_{false};
  bool sniffer_only_{false};
  uint32_t max_frame_length_{128};
  uint32_t in_frame_timeout_ms_{50};

  std::vector<RS485FrameTrigger *> triggers_;
  // Ring buffer: pre-sized and pre-reserved in setup() to avoid per-frame heap allocation.
  // Slots are swapped with tx_frame_buf_ on enqueue (no copy). Max slot size is
  // max_frame_length_ * 2 + FRAME_OVERHEAD_BYTES (worst case: all payload bytes are DLE and
  // must be escaped).
  std::vector<std::vector<uint8_t>> tx_queue_;
  size_t tx_queue_head_{0};   // index of the next slot to read
  size_t tx_queue_tail_{0};   // index of the next slot to write
  size_t tx_queue_count_{0};  // number of frames currently in the ring buffer

  bool in_frame_{false};
  uint8_t previous_byte_{0};
  // True when the immediately preceding in-frame byte was an unescaped DLE whose meaning is
  // still pending. The next byte resolves it: ETX terminates the frame, escape_marker_ marks
  // a stuffed literal DLE, anything else is a protocol violation. Tracking this (rather than
  // peeking at the previous raw byte) is required for doubling mode, where adjacent DLEs
  // would otherwise be miscounted at frame boundaries.
  bool after_dle_{false};
  std::vector<uint8_t> raw_frame_;
  uint32_t last_rx_time_{0};
  bool last_ka_seen_{false};
  uint32_t last_ka_time_{0};
  uint32_t last_keepalive_ms_{0};
  uint32_t last_tx_time_{0};
  // Bus-activity tracking for the idle_gap gate: last_activity_time_ advances on both RX and
  // TX so an idle keepalive we transmit ourselves resets the idle timer (otherwise the gate,
  // which can't hear its own half-duplex transmission, would re-fire every loop). has_tx_ever_
  // is an explicit "has transmitted" flag for the fixed_delay gate so a legitimate now==0 at
  // boot is not mistaken for the never-sent sentinel.
  uint32_t last_activity_time_{0};
  bool has_activity_{false};
  bool has_tx_ever_{false};
  bool tx_start_pending_{false};
  bool pending_is_idle_{false};  // true when pending_tx_frame_ is an idle keepalive, not a real command
  uint32_t tx_start_at_{0};
  std::vector<uint8_t> pending_tx_frame_;  // pre-reserved in setup()

  // Pre-allocated scratch buffers reused each loop to avoid heap churn.
  std::vector<uint8_t> rx_unescaped_;
  std::vector<uint8_t> rx_payload_;
  std::vector<uint8_t> tx_payload_buf_;
  std::vector<uint8_t> tx_escaped_buf_;
  std::vector<uint8_t> tx_frame_buf_;
  // Holds frame_type + payload concatenated for the two-argument queue_raw_frame(); reserved
  // to max_frame_length_ in setup() so the send_frame action / raw button never allocate.
  std::vector<uint8_t> send_assembly_buf_;

  // Setup-time allocated hex-text buffer for dump_frames logging. Sized to fit the
  // worst-case TX frame (max_frame_length_ * 2 + FRAME_OVERHEAD_BYTES bytes fully
  // escaped, 2 hex chars per byte + null). Allocated once in setup() so the log path
  // never touches the heap after that.
  std::unique_ptr<char[]> hex_log_buf_;
  size_t hex_log_buf_size_{0};

  uint32_t frames_received_{0};
  uint32_t crc_failures_{0};
  uint32_t commands_sent_{0};
  uint32_t command_drops_{0};
  // Frames abandoned mid-receive (max_frame_length overflow or intra-frame timeout) -- these
  // never reach validate_frame_(), so they are not counted in crc_failures_.
  uint32_t discarded_frames_{0};
  // Fixed buffer: 4 hex chars for a 2-byte frame type prefix + null terminator. Built-in
  // last_frame_type diagnostic publishes the first two bytes of the payload as hex; longer
  // frame_type prefixes still match correctly but the diagnostic only shows the first two.
  char last_frame_type_[5]{};
};

}  // namespace esphome::rs485_frame

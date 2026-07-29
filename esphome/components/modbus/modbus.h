#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

#include "esphome/components/modbus/modbus_definitions.h"
#include "esphome/components/modbus/modbus_helpers.h"

#include <array>
#include <cstring>
#include <memory>
#include <span>
#include <vector>
#include <deque>
#include <optional>

namespace esphome::modbus {

// Tx queue backstop. Duplicate frames dedup into one entry, so reads can never approach this in a
// sane config - it exists to stop a runaway generator of distinct frames (e.g. a loop writing a
// changing value) from growing the heap unboundedly. The deque grows on demand; this reserves nothing.
// Worst case the cap permits: 128 distinct max-size frames = ~32 kB of spilled frame data plus
// ~3 kB of deque node storage (typical 8-byte frames stay inline; large PDUs spill to one
// allocation each) - pathological configs only, but the numbers matter when tuning for ESP8266.
static constexpr uint16_t MODBUS_TX_BUFFER_SIZE = 128;
static constexpr uint16_t MODBUS_TX_MAX_DELAY_MS = 5;

// Typical frames -- reads and single-register/coil writes -- are exactly 8 bytes
// (address + 5-byte PDU + 2-byte CRC) and fit inline with no heap allocation.
static constexpr uint16_t MODBUS_FRAME_INLINE_SIZE = 8;

struct ModbusFrame {
  // Frame held in a small-buffer-optimized buffer. Typical frames fit inline; only larger
  // multi-register or custom frames spill to a single heap allocation. This keeps the common,
  // high-frequency tx traffic off the heap entirely, avoiding per-frame alloc/free churn.
  // The buffer tracks its own length, so no separate size field is needed.
  SmallInlineBuffer<MODBUS_FRAME_INLINE_SIZE> data;  // Modbus RTU max is 256 bytes

  ModbusFrame(uint8_t address, const uint8_t *pdu, uint16_t pdu_len) {
    uint8_t *buf = this->data.init(pdu_len + 3);
    buf[0] = address;
    memcpy(buf + 1, pdu, pdu_len);
    auto crc = crc16(buf, pdu_len + 1);
    buf[pdu_len + 1] = crc >> 0;
    buf[pdu_len + 2] = crc >> 8;
  }

  uint16_t size() const { return static_cast<uint16_t>(this->data.size()); }

  // A frame is [address][PDU...][CRC lo][CRC hi]. These are the only places that need to know that layout
  uint8_t address() const { return this->data.data()[0]; }
  /// The PDU: function code + data, without address or CRC. Only valid while the frame is alive.
  /// Requires a complete frame (size() >= MIN_FRAME_SIZE, guaranteed by the constructors) - the
  /// subtraction would wrap on anything shorter.
  std::span<const uint8_t> pdu() const { return std::span<const uint8_t>(this->data.data() + 1, this->size() - 3u); }
};

class Modbus : public uart::UARTDevice, public Component {
 public:
  Modbus() = default;

  void setup() override;
  void loop() override;

  float get_setup_priority() const override;
  virtual bool tx_blocked();

  void set_flow_control_pin(GPIOPin *flow_control_pin) { this->flow_control_pin_ = flow_control_pin; }

 protected:
  void receive_bytes_();
  bool timeout_();
  virtual int32_t tx_delay_remaining();
  virtual void parse_modbus_frames() = 0;
  bool parse_modbus_server_frame_();
  // pdu is the whole PDU (function code + payload, no address/CRC); pdu[0] is the (standard or custom) function code.
  virtual void process_modbus_server_frame(uint8_t address, std::span<const uint8_t> pdu) = 0;
  void clear_rx_buffer_(const LogString *reason, bool warn = false, size_t bytes_to_clear = 0);
  bool send_frame_(const ModbusFrame &frame);
  // Scans forward from min_length to find a frame boundary by CRC match for custom function codes.
  // Returns the matched frame length, or 0 if no valid CRC was found within MAX_FRAME_SIZE.
  uint16_t find_custom_frame_end_(uint16_t min_length) const;

  uint32_t last_modbus_byte_{0};
  uint32_t last_receive_check_{0};
  uint32_t last_send_{0};
  uint32_t last_send_tx_offset_{0};
  uint16_t frame_delay_ms_{5};
  uint16_t long_rx_buffer_delay_ms_{0};

  GPIOPin *flow_control_pin_{nullptr};

  std::vector<uint8_t> rx_buffer_;
};

class ModbusClientDevice;
class ModbusServerDevice;

// Transmit ordering class, highest sends first. Internal - derived from the frame and whether the
// entry is a continuous poll, never chosen by callers: mutating function codes outrank one-shot
// reads, which outrank continuous polls (a poll takes whatever bus time is left). Ordering is a
// property of SELECTION (the transmit path picks the best READY entry), not of storage - the
// container is plain append-order.
enum class CommandPriority : uint8_t { CONTINUOUS = 0, READ, WRITE };

/// Lifecycle state of a frame entry. The lifecycle contract on ModbusClientDevice is this
/// transition table:
///   READY -> WAITING            on transmit (send_next_frame_ is the only writer of this transition)
///   WAITING -> RECEIVED_RESPONSE / RECEIVED_EXCEPTION
///                               on a matching data/exception response; on_response()/on_error()
///                               fire AT PARSE TIME (zero-copy spans into the rx buffer), the sweep
///                               then does the lifecycle bookkeeping (pending consumption, continuous
///                               reset - a continuous poll continues only after RECEIVED_RESPONSE -
///                               and removal)
///   WAITING -> TIMED_OUT        on the send-wait timeout; the sweep delivers on_no_response() and
///                               the return value picks retry (-> READY) or resolution
///   WAITING -> INTERRUPTED      on an unexpected-frame interruption; keeps tx blocked until the
///                               send-wait timeout (the old "interrupted shell", now explicit)
///   INTERRUPTED -> INTERRUPTED_NOTIFIED
///                               when the sweep delivers the shell's on_no_response(); a declined
///                               retry resolves one request on the spot (pending--), a granted
///                               retry resolves nothing
///   INTERRUPTED_NOTIFIED -> READY (requests still pending) or removal (none), when the
///                               send-wait timeout releases the shell
///   anything -> DELETED         on clear_tx_queue_* (a clear is a state flip; the sweep resolves
///                               owed terminals - or removes silently for device-scoped clears)
///   WAITING -> WAITING_DELETED  on a clear with clear_sent: the frame is on the wire, so it
///                               persists as a response-ignoring shell, -> DELETED on response or
///                               timeout and swept silently
///   READY -> REFUSED            on a transmit failure: the entry owes exactly one on_not_sent(),
///                               delivered by the sweep; remaining pending then returns the entry
///                               to READY for the absorbed request's run. (Requests that never
///                               enter the machine - empty/oversize PDU, full queue, anonymous
///                               duplicate - get NO callback: send_pdu() returns false instead.)
/// THE SWEEP (run each loop() between the watchdog and transmit, armed by sweep_needed_) walks
/// entries needing service and delivers their callbacks from a QUIESCENT hub - terminal-state
/// entries ARE the pending deliveries, so no separate notification queue exists and no callback
/// ever runs mid-mutation. It serves only the entries that existed when it began, so a callback
/// that queues a frame - or queues one and then clears it - cannot add to the work being served;
/// anything new is picked up by the next sweep. That bound is the whole termination argument.
enum class FrameState : uint8_t {
  READY = 0,
  WAITING,
  RECEIVED_RESPONSE,
  RECEIVED_EXCEPTION,
  TIMED_OUT,
  INTERRUPTED,
  INTERRUPTED_NOTIFIED,
  REFUSED,
  WAITING_DELETED,
  DELETED,
};

/// Per-command options for the send helpers. Extended append-only; prefer designated initializers
/// at call sites ({.continuous = true}) so added fields never disturb existing callers.
struct CommandOptions {
  /// Register this read as a CONTINUOUS poll: it lives in the queue until cancelled, cycling
  /// READY -> WAITING -> RECEIVED_RESPONSE -> READY, selected least-recently-served whenever no one-shot
  /// wants the bus. A failure (timeout without retry, transmit failure) ends the poll; the
  /// caller's normal update path recovers. Ignored for mutating function codes.
  bool continuous{false};
};

struct ModbusDeviceCommand {
  ModbusClientDevice *device;
  ModbusFrame frame;
  FrameState state{FrameState::READY};
  /// CONTINUOUS entries are subscriptions: never consumed by completion, pending
  /// fixed at 1 ("the subscription"), removed only by cancellation or failure.
  bool continuous{false};
  /// Accepted requests this entry stands for, never more than it can serve: a duplicate is
  /// absorbed only while pending is below the servable cap (2 for requeueable reads - this run
  /// plus one re-run - and 1 for everything else), and refused at the door otherwise. An entry
  /// leaving the world drains to zero, one terminal per remaining count.
  uint8_t pending{1};
  /// Place-in-line stamp from the hub's free-running counter: set when the entry is first queued,
  /// left alone when a duplicate is absorbed (pending++), and re-stamped whenever the entry
  /// re-enters the line - a request resolving (pending--), a granted retry (resolve-then-
  /// re-request), or a continuous poll completing its cycle. Selection compares AGE against the
  /// current counter (see select_next_ready_) and takes the oldest, so within a class the queue is
  /// round-robin fair: a re-attempt goes behind requests that arrived while the frame was waiting.
  /// The counter is meant to wrap; ordering only degrades if one entry waits through 65536 counter
  /// advances, which needs higher-class traffic starving it for that long.
  uint16_t seq{0};

  ModbusDeviceCommand(ModbusClientDevice *device, uint8_t address, const uint8_t *src, uint16_t len)
      : device(device), frame(address, src, len) {}
  /// Build a command from a PDU span. Callers must bound the PDU to MAX_PDU_SIZE.
  ModbusDeviceCommand(ModbusClientDevice *device, uint8_t address, std::span<const uint8_t> pdu)
      : device(device), frame(address, pdu.data(), static_cast<uint16_t>(pdu.size())) {}

  /// Transmit ordering class, derived - never stored, so it can never disagree with the entry it
  /// describes. A continuous poll ranks below every one-shot; otherwise the class comes from the
  /// function code alone.
  CommandPriority priority() const {
    return this->continuous ? CommandPriority::CONTINUOUS : classify(this->frame.pdu()[0]);
  }
  /// The wire-derived half: mutating function codes rank WRITE; exception-flagged codes never do
  /// (is_function_code_write() masks the exception bit, so they are excluded up front).
  static CommandPriority classify(uint8_t function_code) {
    if (helpers::is_function_code_exception(function_code))
      return CommandPriority::READ;
    const auto code = static_cast<FunctionCode>(function_code);
    if (helpers::is_function_code_write(function_code) || code == FunctionCode::MASK_WRITE_REGISTER ||
        code == FunctionCode::READ_WRITE_MULTIPLE_REGISTERS) {
      return CommandPriority::WRITE;
    }
    return CommandPriority::READ;
  }

  /// How many requests this entry can ever serve: a standard read runs twice (this run plus one
  /// re-run), everything else once - a write is not idempotent, and a custom or diagnostic code's
  /// idempotency is unknown. Duplicates are absorbed below this and refused at it.
  uint8_t servable_cap() const {
    const uint8_t fc = this->frame.pdu()[0];
    const bool requeueable = !helpers::is_function_code_exception(fc) && helpers::is_function_code_read(fc);
    return (requeueable && !this->continuous) ? 2 : 1;
  }
  /// Retire this entry: no callbacks owed from here on. DELETED with pending 0 is the silent,
  /// erasable terminal; the sweep's erase pass removes it.
  void retire() {
    this->state = FrameState::DELETED;
    this->pending = 0;
  }

  /// True if this command carries the same wire frame (address + PDU) as the given one.
  bool same_frame(uint8_t address, std::span<const uint8_t> pdu) const {
    const auto own_pdu = this->frame.pdu();
    return own_pdu.size() == pdu.size() && this->frame.address() == address &&
           memcmp(own_pdu.data(), pdu.data(), pdu.size()) == 0;
  }
};

class ModbusClientHub : public Modbus {
 public:
  ModbusClientHub() = default;
  void dump_config() override;
  void loop() override;
  void set_send_wait_time(uint16_t time_in_ms) { this->send_wait_time_ = time_in_ms; }
  void set_turnaround_time(uint16_t time_in_ms) { this->turnaround_delay_ms_ = time_in_ms; }
  bool tx_buffer_empty();
  bool tx_blocked() override;
  ESPDEPRECATED("Use send_pdu() with create_client_pdu() instead. Removed in 2026.10.0", "2026.4.0")
  void send(uint8_t address, uint8_t function_code, uint16_t start_address, uint16_t number_of_entities,
            uint8_t payload_len = 0, const uint8_t *payload = nullptr, ModbusClientDevice *device = nullptr) {
    this->send_pdu(address,
                   helpers::create_client_pdu((FunctionCode) function_code, start_address, number_of_entities, payload,
                                              payload_len),
                   device);
  };
  /// Queue a request. Returns true when the request is now represented by a live entry (queued
  /// fresh, or absorbed into an identical one) and will resolve in exactly one terminal callback.
  /// Returns false when it never entered the machine - empty or oversize PDU, full queue, an
  /// anonymous duplicate, or a duplicate of an entry already holding as many requests as it can
  /// serve - in which case NO callback follows: the refusal is the return value.
  bool send_pdu(uint8_t address, std::span<const uint8_t> pdu, ModbusClientDevice *device = nullptr,
                CommandOptions options = {});
  ESPDEPRECATED("Use send_pdu(payload[0], <pdu bytes>, device) instead. Removed in 2027.2.0", "2026.8.0")
  void send_raw(const std::vector<uint8_t> &payload, ModbusClientDevice *device = nullptr);
  // Drop the queued commands for an address; every dropped frame resolves via its owner's on_not_sent()
  // at the next sweep, so other devices sharing the address observe the drop. The waiting frame is
  // only detached (silently) when clear_sent is set. clear_tx_queue_for_device() SILENTLY discards the
  // caller's own frames (supersede/teardown semantics); see the lifecycle note on ModbusClientDevice.
  void clear_tx_queue_for_address(uint8_t address, bool clear_sent = true);
  void clear_tx_queue_for_device(ModbusClientDevice *device);

 protected:
  int32_t tx_delay_remaining() override;
  void parse_modbus_frames() override;
  void process_modbus_server_frame(uint8_t address, std::span<const uint8_t> pdu) override;
  void send_next_frame_();
  // Deliver owed callbacks from a quiescent hub and apply lifecycle bookkeeping; see FrameState.
  void sweep_();
  // The selection function: best READY entry (WRITE class first, then one-shot reads, then the
  // least-recently-served continuous; FIFO by seq within each group), or nullptr.
  ModbusDeviceCommand *select_next_ready_();
  // Locate the single WAITING/INTERRUPTED[_NOTIFIED]/WAITING_DELETED entry (the frame on the wire).
  ModbusDeviceCommand *find_waiting_();
  // End the wait for a response on send-wait timeout (the loop() watchdog body); see FrameState.
  void expire_waiting_();
  // Entries counted against the transmit-queue cap: excludes REFUSED (they occupy the reserve)
  // and the *_DELETED states (already resolved, awaiting the erase pass).
  size_t live_count_() const;

  uint16_t send_wait_time_{2000};
  uint16_t turnaround_delay_ms_{0};

  /// Tracking the frame on the wire: cache the one VALUE the loop rate needs, derive everything
  /// else by scan at event rate (the watchdog only looks the entry up once the send-wait timer
  /// has already expired). Uniqueness is enforced at the transmit gate: waiting_for_response_ is
  /// written at exactly two kinds of sites (set on transmit, cleared on the transaction-ending
  /// transition) and send_next_frame_ refuses to select while it is set.
  bool waiting_for_response_{false};

  /// Set whenever a transition leaves owed callbacks behind; quiet loop() passes skip the walk
  /// when clear.
  bool sweep_needed_{false};
  /// Monotonic stamp source for ModbusDeviceCommand::seq.
  uint16_t next_seq_{0};

  // Plain append-order container; ordering lives in select_next_ready_(), lifecycle in FrameState.
  std::deque<ModbusDeviceCommand> tx_buffer_;
};

class ModbusServerHub : public Modbus {
 public:
  ModbusServerHub() = default;
  void dump_config() override;
  void register_device(ModbusServerDevice *device) { this->devices_.push_back(device); }

 protected:
  void parse_modbus_frames() override;
  bool parse_modbus_client_frame_();
  void process_modbus_server_frame(uint8_t address, std::span<const uint8_t> pdu) override;
  void process_modbus_client_frame_(uint8_t address, uint8_t function_code, const uint8_t *data);
  ModbusServerDevice *find_device_(uint8_t address);
  // Returns true if [start_address, start_address + number_of_registers) fits in the 16-bit address space.
  // On failure, logs and sends an ILLEGAL_DATA_ADDRESS exception to the client.
  bool check_register_range_(uint8_t address, uint8_t function_code, uint16_t start_address,
                             uint16_t number_of_registers);
  void send_raw_(const uint8_t *payload, uint16_t len);
  void send_exception_(uint8_t address, uint8_t function_code, ExceptionCode exception_code);
  void send_response_(uint8_t address, uint8_t function_code, const uint8_t *payload, uint16_t payload_len);
  uint8_t expecting_peer_response_{0};
  std::vector<ModbusServerDevice *> devices_;

  // Holds the raw payload of a single reply deferred for sending when tx was blocked at send time.
  // Only one server reply can be waiting at once, so a single fixed buffer avoids heap allocation.
  std::array<uint8_t, MAX_RAW_SIZE> deferred_payload_;
  uint16_t deferred_payload_len_{0};
};

// Transaction status: std::nullopt on success, otherwise a Modbus exception code
using ResponseStatus = std::optional<ExceptionCode>;

/// Command lifecycle: each accepted request (a send_pdu()/typed-helper call, a retry granted by
/// on_no_response(), or a continuous poll's cycle) ends in exactly ONE terminal callback:
/// on_response() (valid response), on_error() (exception response), on_no_response()
/// (timeout or interrupted transaction), or on_not_sent() (accepted but never transmitted: a
/// transmit failure or a drop by clear_tx_queue_for_address()).
/// Requests that never enter the machine get NO callback - send_pdu() returns false at the call
/// site instead. on_sent() is additional, not terminal: it fires once per wire transmission,
/// before whichever of data/error/no_response follows, and never for a request that ends in
/// on_not_sent().
/// Delivery timing: on_response()/on_error() fire at parse time (zero-copy spans); every other
/// callback is delivered by the sweep during the hub's loop(), from a quiescent hub -
/// see the FrameState transition table for the full machine.
/// The exceptions to "exactly one terminal":
///  - clear_tx_queue_for_device() drops the caller's OWN commands SILENTLY (supersede/teardown
///    semantics), and both clear variants detach the waiting frame - and a command mid-completion
///    (inside its own response callbacks) - silently, cancelling any pending re-run or poll cycle.
///    clear_tx_queue_for_address() DOES resolve every queued frame it drops via the owner's
///    on_not_sent(), one delivery per accepted request the entry stood for.
///  - a duplicate absorbed into a CONTINUOUS entry is served uncounted by the poll's
///    next response; if the poll dies before that response, its single terminal stands for those
///    requests as well. Likewise, converting an entry to continuous ({.continuous = true} matching
///    a queued frame) SUPERSEDES any request that entry had absorbed: the caller opted into
///    streaming semantics, and the poll's responses are the accounting from then on.
/// Sending from inside a callback is safe but check the return value; the new frame takes no part
/// in the sweep that is running and is picked up by the next one. An address-scoped clear resolves
/// EVERY dropped request with its own terminal at the sweep, the clearer's included; use
/// clear_tx_queue_for_device() when you want your own frames torn down silently.
class ModbusClientDevice {
 public:
  ModbusClientDevice() = default;
  ModbusClientDevice(ModbusClientHub *parent, uint8_t address) : parent_(parent), address_(address) {}
  virtual ~ModbusClientDevice() {
    if (this->parent_ != nullptr)
      this->clear_tx_queue_for_device();
  }
  ModbusClientDevice(const ModbusClientDevice &) = delete;
  ModbusClientDevice &operator=(const ModbusClientDevice &) = delete;
  ModbusClientDevice(ModbusClientDevice &&) = delete;
  ModbusClientDevice &operator=(ModbusClientDevice &&) = delete;
  void set_parent(ModbusClientHub *parent) { this->parent_ = parent; }
  void set_address(uint8_t address) { this->address_ = address; }
  /// Low-level response hook: called with the request PDU this device sent and the response PDU received
  /// The spans are only valid for the duration of the call - copy the bytes if they must outlive it.
  /// The default implementation decodes standard responses and dispatches to on_read_* / on_write_* callbacks below.
  /// Override it to handle raw PDUs directly.
  virtual void on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) {
    this->dispatch_response_(request_pdu, response_pdu, std::nullopt);
  }
  /// Low-level error hook: called with the request PDU and the modbus exception code from the error response.
  /// The default implementation dispatches to the same typed callbacks with the exception code as status.
  /// Devices implementing the High-level typed callbacks see success and failure through one interface.
  virtual void on_error(std::span<const uint8_t> request_pdu, ExceptionCode exception_code) {
    this->dispatch_response_(request_pdu, {}, exception_code);
  }
  /// Called when an ACCEPTED request could not be transmitted (a transmit failure, or a drop by
  /// clear_tx_queue_for_address()). A send that is refused outright never gets this callback -
  /// send_pdu() returns false instead. Re-sending from inside this callback is safe: check the
  /// return value (see the contract above).
  /// (The on_modbus_* names below are deprecated pre-rename spellings; the defaults forward so
  /// external devices overriding them keep working through the deprecation window.)
  virtual void on_not_sent(std::span<const uint8_t> request_pdu) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    this->on_modbus_not_sent();
#pragma GCC diagnostic pop
  }
  /// Called when this device's frame is actually written to the wire
  virtual void on_sent(std::span<const uint8_t> request_pdu) {}
  /// Called when no matching, uninterrupted response arrived; return true to have the hub re-queue the frame for a
  /// retry. The hub does not bound retries: the device is responsible for limiting them.
  virtual bool on_no_response(std::span<const uint8_t> request_pdu) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    return this->on_modbus_no_response();
#pragma GCC diagnostic pop
  }
  // Remove before 2027.2.0
  ESPDEPRECATED("Override on_not_sent() instead. Removed in 2027.2.0", "2026.8.0")
  virtual void on_modbus_not_sent() {}
  // Remove before 2027.2.0
  ESPDEPRECATED("Override on_no_response() instead. Removed in 2027.2.0", "2026.8.0")
  virtual bool on_modbus_no_response() { return false; }

  /// High-level typed response callbacks, fired by the default on_response()/on_error() with arguments
  /// parsed from the request and response PDUs.
  /// Status is std::nullopt on success; holds the exception code on failure.
  /// Register values are in host byte order; spans are only valid for the duration of the call.
  virtual void on_read_registers(EntityType entity_type, uint16_t start_address, std::span<const uint16_t> registers,
                                 ResponseStatus status) {}
  virtual void on_read_holding_registers(uint16_t start_address, std::span<const uint16_t> registers,
                                         ResponseStatus status) {
    this->on_read_registers(EntityType::HOLDING, start_address, registers, status);
  }
  virtual void on_read_input_registers(uint16_t start_address, std::span<const uint16_t> registers,
                                       ResponseStatus status) {
    this->on_read_registers(EntityType::INPUT_REGISTER, start_address, registers, status);
  }
  /// Coil/discrete-input reads are delivered as a PackedBits view (bit 0 = the bit at start_address,
  /// bits.size() = the count requested). The view points into the hub's receive buffer and is only
  /// valid during the call.
  virtual void on_read_bits(EntityType entity_type, uint16_t start_address, PackedBits bits, ResponseStatus status) {}
  virtual void on_read_coils(uint16_t start_address, PackedBits bits, ResponseStatus status) {
    this->on_read_bits(EntityType::COIL, start_address, bits, status);
  }
  virtual void on_read_discrete_inputs(uint16_t start_address, PackedBits bits, ResponseStatus status) {
    this->on_read_bits(EntityType::DISCRETE_INPUT, start_address, bits, status);
  }
  /// Write acknowledgements. These deliberately mirror the read callbacks' shapes, so a write ack can be fed
  /// through the same handler as a read (registers.size() / bits.size() gives the count)
  ///
  /// IMPORTANT - for the multi-writes these are the values that were REQUESTED, not device-confirmed
  /// state: a multi-write ack only echoes the start address and count, so the values are decoded from
  /// the request PDU, and they are delivered even when status holds an exception code. Always check
  /// status, and treat publishing them as an optimistic update rather than a read-back. The single
  /// writes are the exception: their successful ack echoes the value, so on success the delivered
  /// value is the device's echo (on an exception it falls back to the request copy).
  virtual void on_write_single_register(uint16_t address, uint16_t value, ResponseStatus status) {}
  virtual void on_write_single_coil(uint16_t address, bool value, ResponseStatus status) {}
  virtual void on_write_multiple_registers(uint16_t start_address, std::span<const uint16_t> registers,
                                           ResponseStatus status) {}
  virtual void on_write_multiple_coils(uint16_t start_address, PackedBits bits, ResponseStatus status) {}
  /// Catch-all for custom function codes and anything that is not a standard-conformant transaction
  /// (see dispatch_response_()); on failure the response is empty and the exception code is in status.
  /// The default implementation only logs a warning that the response is going unhandled - override it
  /// to handle custom traffic (which also silences the warning).
  virtual void on_custom_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu,
                                  ResponseStatus status);
  ESPDEPRECATED("Use the typed read_*/write_* helpers or send_pdu() instead. Removed in 2027.2.0", "2026.8.0")
  void send(uint8_t function, uint16_t start_address, uint16_t number_of_entities, uint8_t payload_len = 0,
            const uint8_t *payload = nullptr) {
    this->parent_->send_pdu(
        this->address_,
        helpers::create_client_pdu((FunctionCode) function, start_address, number_of_entities, payload, payload_len),
        this);
  }
  /// See ModbusClientHub::send_pdu(): true = accepted (a terminal callback will follow),
  /// false = refused at the door (no callback).
  bool send_pdu(std::span<const uint8_t> pdu, CommandOptions options = {}) {
    return this->parent_->send_pdu(this->address_, pdu, this, options);
  }
  ESPDEPRECATED("Use send_pdu() instead (the device address is prepended for you). Removed in 2027.2.0", "2026.8.0")
  bool send_raw(const std::vector<uint8_t> &payload) {
    if (payload.empty())
      return false;  // too short to contain a PDU; refused at the door like any invalid send
    return this->parent_->send_pdu(payload[0], std::span<const uint8_t>(payload).subspan(1), this);
  }
  // Reads via the table-appropriate function code; an unreadable entity type maps to INVALID, which
  // create_read_pdu() rejects into an empty PDU and send_pdu() refuses with a false return.
  bool read_entities(EntityType entity_type, uint16_t start_address, uint16_t number_of_entities,
                     CommandOptions options = {}) {
    return this->send_pdu(helpers::create_read_pdu(helpers::modbus_register_read_function(entity_type), start_address,
                                                   number_of_entities),
                          options);
  }
  bool read_input_registers(uint16_t start_address, uint16_t number_of_registers, CommandOptions options = {}) {
    return this->send_pdu(
        helpers::create_read_pdu(FunctionCode::READ_INPUT_REGISTERS, start_address, number_of_registers), options);
  }
  bool read_holding_registers(uint16_t start_address, uint16_t number_of_registers, CommandOptions options = {}) {
    return this->send_pdu(
        helpers::create_read_pdu(FunctionCode::READ_HOLDING_REGISTERS, start_address, number_of_registers), options);
  }
  bool read_coils(uint16_t start_address, uint16_t number_of_coils, CommandOptions options = {}) {
    return this->send_pdu(helpers::create_read_pdu(FunctionCode::READ_COILS, start_address, number_of_coils), options);
  }
  bool read_discrete_inputs(uint16_t start_address, uint16_t number_of_inputs, CommandOptions options = {}) {
    return this->send_pdu(helpers::create_read_pdu(FunctionCode::READ_DISCRETE_INPUTS, start_address, number_of_inputs),
                          options);
  }
  bool write_single_register(uint16_t start_address, uint16_t value) {
    return this->send_pdu(helpers::create_write_single_register_pdu(start_address, value));
  }
  bool write_single_coil(uint16_t address, bool value) {
    return this->send_pdu(helpers::create_write_single_coil_pdu(address, value));
  }
  bool write_multiple_registers(uint16_t start_address, std::span<const uint16_t> values) {
    return this->send_pdu(helpers::create_write_registers_pdu(start_address, values));
  }
  /// Note: std::vector<bool> cannot bind to std::span<const bool>; use a contiguous bool container or the packed
  /// overload.
  bool write_multiple_coils(uint16_t start_address, std::span<const bool> values) {
    return this->send_pdu(helpers::create_write_coils_pdu(start_address, values));
  }
  /// Packed variant: a PackedBits view (the same layout on_read_coils() delivers), so
  /// read-modify-write needs no unpack/repack.
  bool write_multiple_coils(uint16_t start_address, PackedBits bits) {
    return this->send_pdu(helpers::create_write_coils_pdu(start_address, bits));
  }
  inline void clear_tx_queue_for_address(bool clear_sent = true) {
    this->parent_->clear_tx_queue_for_address(this->address_, clear_sent);
  }
  inline void clear_tx_queue_for_device() { this->parent_->clear_tx_queue_for_device(this); }

  // If more than one device is connected block sending a new command before a response is received
  ESPDEPRECATED("Use ready_for_immediate_send() instead. Removed in 2026.9.0", "2026.3.0")
  bool waiting_for_response() { return !this->ready_for_immediate_send(); }
  bool ready_for_immediate_send() { return this->parent_->tx_buffer_empty() && !this->parent_->tx_blocked(); }

 protected:
  /// Parses the request/response PDU pair and dispatches to the matching high-level typed callback
  void dispatch_response_(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu,
                          ResponseStatus status);

  ModbusClientHub *parent_{nullptr};
  uint8_t address_{0};
  bool custom_response_warned_{false};  // first unhandled custom response warns; repeats log at VERBOSE
};

// Compatibility shim for external components written against the pre-2026.8 API, which subclassed
// ModbusDevice and overrode on_modbus_data()/on_modbus_error(). The name is free (nothing in-tree
// uses it), so instead of a plain alias it adapts the new span-based hooks back to the old
// signatures: on_modbus_data() receives the response payload as an owning vector (the heap copy
// exists only on this deprecated path) and on_modbus_error() the function code and exception code.
// Remove before 2027.2.0 (window restarted when the plain alias became a behavior shim in 2026.8.0)
class ESPDEPRECATED("Subclass ModbusClientDevice and override on_response()/on_error() instead. Removed in 2027.2.0",
                    "2026.8.0") ModbusDevice : public ModbusClientDevice {
 public:
  using ModbusClientDevice::ModbusClientDevice;
  virtual void on_modbus_data(const std::vector<uint8_t> &data) {}
  virtual void on_modbus_error(uint8_t function_code, uint8_t exception_code) {}

  void on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) override {
    auto payload = helpers::server_pdu_payload(response_pdu);
    this->on_modbus_data(std::vector<uint8_t>(payload.begin(), payload.end()));
  }
  void on_error(std::span<const uint8_t> request_pdu, ExceptionCode exception_code) override {
    this->on_modbus_error(request_pdu.empty() ? 0 : request_pdu[0], static_cast<uint8_t>(exception_code));
  }
};

// Register values exchanged with server handlers, in host byte order. Sized at the larger of the two protocol
// maxima (read = 125 / 0x7D, write = 123 / 0x7B); the per-direction count limit is enforced by the hub, not by
// the capacity of this type.
using RegisterValues = StaticVector<uint16_t, MAX_NUM_OF_REGISTERS_TO_READ>;

class ModbusServerDevice {
 public:
  virtual ~ModbusServerDevice() = default;
  ModbusServerDevice() = default;
  // Polymorphic base: non-copyable and non-movable to prevent slicing (Rule of Five).
  ModbusServerDevice(const ModbusServerDevice &) = delete;
  ModbusServerDevice &operator=(const ModbusServerDevice &) = delete;
  ModbusServerDevice(ModbusServerDevice &&) = delete;
  ModbusServerDevice &operator=(ModbusServerDevice &&) = delete;
  void set_address(uint8_t address) { this->address_ = address; }
  uint8_t get_address() const { return this->address_; }
  virtual ResponseStatus on_read_registers(uint16_t start_address, uint16_t number_of_registers,
                                           RegisterValues &registers) {
    return ExceptionCode::ILLEGAL_FUNCTION;
  };
  virtual ResponseStatus on_read_input_registers(uint16_t start_address, uint16_t number_of_registers,
                                                 RegisterValues &registers) {
    return this->on_read_registers(start_address, number_of_registers, registers);
  };
  virtual ResponseStatus on_read_holding_registers(uint16_t start_address, uint16_t number_of_registers,
                                                   RegisterValues &registers) {
    return this->on_read_registers(start_address, number_of_registers, registers);
  };
  virtual ResponseStatus on_write_registers(uint16_t start_address, const RegisterValues &registers) {
    return ExceptionCode::ILLEGAL_FUNCTION;
  };

 protected:
  uint8_t address_{0};
};

}  // namespace esphome::modbus

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

// Tx queue backstop: duplicates dedup into one entry, so only a runaway generator of distinct frames
// (e.g. a loop writing a changing value) could grow the heap unboundedly.
static constexpr uint16_t MODBUS_TX_BUFFER_SIZE = 128;
static constexpr uint16_t MODBUS_TX_MAX_DELAY_US = 5000;

// Typical frames -- reads and single-register/coil writes -- are exactly 8 bytes
// (address + 5-byte PDU + 2-byte CRC).
static constexpr uint16_t MODBUS_FRAME_INLINE_SIZE = 8;

struct ModbusFrame {
  // Small-buffer-optimized: typical frames fit inline, keeping high-frequency tx traffic off the
  // heap; only large multi-register or custom frames spill to a single heap allocation.
  SmallInlineBuffer<MODBUS_FRAME_INLINE_SIZE> data;

  // A frame is [address][PDU...][CRC lo][CRC hi]. These are the only places that need to know that layout
  ModbusFrame(uint8_t address, const uint8_t *pdu, uint16_t pdu_len) {
    uint8_t *buf = this->data.init(pdu_len + 3);
    buf[0] = address;
    memcpy(buf + 1, pdu, pdu_len);
    auto crc = crc16(buf, pdu_len + 1);
    buf[pdu_len + 1] = crc >> 0;
    buf[pdu_len + 2] = crc >> 8;
  }

  uint16_t size() const { return static_cast<uint16_t>(this->data.size()); }
  uint8_t address() const { return this->data.data()[0]; }
  /// A PDU is [function code][data...] without address or CRC. Only valid while the frame is alive.
  /// Requires a complete frame (size() >= MIN_FRAME_SIZE, guaranteed by the constructors)
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
  virtual void process_modbus_server_frame(uint8_t address, std::span<const uint8_t> pdu) = 0;
  void clear_rx_buffer_(const LogString *reason, bool warn = false, size_t bytes_to_clear = 0);
  bool send_frame_(const ModbusFrame &frame);
  uint16_t find_frame_end_by_crc_(uint16_t min_length) const;

  // All timestamps and durations below are micros()-based
  uint32_t last_modbus_byte_{0};
  uint32_t last_receive_check_{0};
  uint32_t last_send_{0};
  uint32_t last_send_tx_offset_{0};
  uint32_t frame_delay_us_{5000};
  uint32_t long_rx_buffer_delay_us_{0};
  uint32_t rx_detect_latency_us_{0};
  // Bits on the wire per character (start + data + optional parity + stop); 12 at most.
  uint8_t bits_per_char_{11};
  // Latched when a read reaches rx_full_threshold, cleared when the buffer drains.
  bool exceeded_rx_full_threshold_{false};

  GPIOPin *flow_control_pin_{nullptr};

  std::vector<uint8_t> rx_buffer_;
};

class ModbusClientDevice;
class ModbusServerDevice;

// Transmit ordering, highest first: writes before one-shot reads before continuous polls.
enum class CommandPriority : uint8_t { CONTINUOUS = 0, READ, WRITE };

// Per-entry lifecycle state. Waiting states (see waiting_state()) hold the bus; the sweep delivers owed
// callbacks from a quiescent hub, and an entry is erased once pending == 0 && !waiting_state().
enum class FrameState : uint8_t {
  READY = 0,
  WAITING,
  RECEIVED_RESPONSE,
  RECEIVED_EXCEPTION,
  TIMED_OUT,            // on_no_response delivered at the send-wait timeout; awaiting reschedule/erase
  INTERRUPTED,          // unexpected frame arrived; ignores this transaction, waits out the timeout
  WAITING_RETIRED,      // retired while WAITING: a late response is still delivered as its usual terminal
  INTERRUPTED_RETIRED,  // retired while INTERRUPTED: still distrusts late frames, ends in on_no_response
  RETIRED,              // retired, off the wire
};

// Per-command send options. Append-only; pass via designated initializers ({.continuous = true}).
// A new field reaches the queue with no plumbing but arrives inert until it defines three rules:
// normalization in queue_pdu(), a merge rule for duplicate absorption, and teardown in
// retire()/silent_retire().
struct CommandOptions {
  // A continuous poll lives in the queue until cancelled or failed; ignored for mutating codes.
  bool continuous{false};
};

struct ModbusDeviceCommand {
  ModbusClientDevice *device;
  ModbusFrame frame;
  // Place-in-line stamp (hub's free-running counter); selection takes the oldest for round-robin
  // fairness within a class. Meant to wrap.
  uint16_t seq{0};
  FrameState state{FrameState::READY};
  // Accepted requests this entry stands for, capped at max_pending(); drains one terminal each.
  // A continuous poll is a subscription: pending fixed at 1, removed only by cancellation or failure.
  uint8_t pending{1};
  // The entry's LIVE effective options, not a record of the caller's request
  CommandOptions options;

  // Build a command from a PDU span (caller bounds it to MAX_PDU_SIZE) and pre-normalized options;
  // fully initialized here.
  ModbusDeviceCommand(ModbusClientDevice *device, uint8_t address, std::span<const uint8_t> pdu,
                      CommandOptions options = {}, uint16_t seq = 0)
      : device(device), frame(address, pdu.data(), static_cast<uint16_t>(pdu.size())), seq(seq), options(options) {}

  CommandPriority priority() const {
    if (this->options.continuous)
      return CommandPriority::CONTINUOUS;
    if (helpers::is_function_code_write(this->frame.pdu()[0])) {
      return CommandPriority::WRITE;
    }
    return CommandPriority::READ;
  }

  // Requests this entry can serve
  uint8_t max_pending() const {
    const uint8_t fc = this->frame.pdu()[0];
    return (helpers::is_function_code_read_only(fc) && !this->options.continuous) ? 2 : 1;
  }
  // Device-scoped clear: detach with no callback. An entry still waiting for a response keeps its state as a
  // reply-ignoring shell that resolves silently; any other goes RETIRED.
  void silent_retire() {
    if (!this->waiting_state())
      this->state = FrameState::RETIRED;
    this->pending = 0;
    this->device = nullptr;
  }
  // Fire-and-forget completion for a broadcast (address 0): the frame was transmitted (on_sent already
  // fired), but a broadcast is never answered (Modbus 4.1), so the entry retires with no terminal callback.
  void complete_broadcast() {
    this->state = FrameState::RETIRED;
    this->pending = 0;
  }
  // Re-ready for another transmission, restamped to the tail of its class
  void requeue(uint16_t seq) {
    this->state = FrameState::READY;
    this->seq = seq;
  }
  // Re-task a frame that lives on: upgrade a one-shot to a continuous poll, or downgrade a poll back to
  // a one-shot.
  void make_continuous(bool continuous) {
    if (continuous) {
      this->options.continuous = true;
      this->pending = 1;
    } else {
      this->increment_pending();
      this->options.continuous = false;
    }
  }
  // Address-scoped clear: keep pending and device so the sweep delivers one on_not_sent() per un-delivered
  // request. An entry still waiting for a response keeps its in-flight request (whose usual terminal is
  // still coming) and drains only its duplicates.
  void retire() {
    if (this->state == FrameState::WAITING) {
      this->state = FrameState::WAITING_RETIRED;
    } else if (this->state == FrameState::INTERRUPTED) {
      this->state = FrameState::INTERRUPTED_RETIRED;
    } else if (!this->waiting_state()) {  // an already-retired shell stays put; off the wire -> RETIRED
      this->state = FrameState::RETIRED;
    }
    this->options = {};  // reset every option
  }

  // True while the entry is still waiting for a response
  bool waiting_state() const {
    return this->state == FrameState::WAITING || this->state == FrameState::INTERRUPTED ||
           this->state == FrameState::WAITING_RETIRED || this->state == FrameState::INTERRUPTED_RETIRED;
  }

  bool decrement_pending() {
    if (this->pending > 0) {
      this->pending--;
      return true;
    }
    return false;
  }

  bool increment_pending() {
    if (this->pending < this->max_pending()) {
      this->pending++;
      return true;
    }
    return false;
  }

  // Terminal/lifecycle methods: each owns its transition, callback, and pending accounting and
  // returns whether a callback ran.
  bool sent();
  bool response(std::span<const uint8_t> response_pdu);
  bool error(ExceptionCode exception_code);
  bool interrupt();
  bool timed_out();
  bool notify_retired();

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
  // Config arrives in milliseconds; stored internally in microseconds like all other timing.
  void set_send_wait_time(uint16_t time_in_ms) { this->send_wait_time_us_ = time_in_ms * 1000UL; }
  void set_turnaround_time(uint16_t time_in_ms) { this->turnaround_delay_us_ = time_in_ms * 1000UL; }
  bool tx_buffer_empty();
  bool tx_blocked() override;
  ESPDEPRECATED("Use queue_pdu() with create_client_pdu() instead. Removed in 2026.10.0", "2026.4.0")
  void send(uint8_t address, uint8_t function_code, uint16_t start_address, uint16_t number_of_entities,
            uint8_t payload_len = 0, const uint8_t *payload = nullptr, ModbusClientDevice *device = nullptr) {
    this->queue_pdu(address,
                    helpers::create_client_pdu((FunctionCode) function_code, start_address, number_of_entities, payload,
                                               payload_len),
                    device);
  };
  /// Queue a request. True = accepted: it resolves in exactly one terminal callback (a broadcast,
  /// address 0, gets only on_sent()). False = refused, and no callback of any kind follows.
  /// Neither means anything reached the wire - on_sent() reports that.
  bool queue_pdu(uint8_t address, std::span<const uint8_t> pdu, ModbusClientDevice *device = nullptr,
                 CommandOptions options = {});
  // Remove before 2027.2.0. Deliberately the void, no-options signature 2026.7.4 shipped: nothing
  // external can rely on the later additions under this name.
  ESPDEPRECATED("Use queue_pdu() instead - the call queues a request, it does not send one, and it "
                "reports whether the request was accepted. Removed in 2027.2.0",
                "2026.8.0")
  void send_pdu(uint8_t address, std::span<const uint8_t> pdu, ModbusClientDevice *device = nullptr) {
    this->queue_pdu(address, pdu, device);
  }
  ESPDEPRECATED("Use queue_pdu(payload[0], <pdu bytes>, device) instead. Removed in 2027.2.0", "2026.8.0")
  void send_raw(const std::vector<uint8_t> &payload, ModbusClientDevice *device = nullptr);
  // Clear all commands matching the given address; each unsent request resolves via on_not_sent(), but a
  // frame on the wire still runs to its usual terminal.
  void clear_tx_queue_for_address(uint8_t address);
  // Clear all commands for a given device; no callbacks are delivered.
  void clear_tx_queue_for_device(ModbusClientDevice *device);

 protected:
  int32_t tx_delay_remaining() override;
  void parse_modbus_frames() override;
  void process_modbus_server_frame(uint8_t address, std::span<const uint8_t> pdu) override;
  void send_next_frame_();
  // Deliver owed callbacks from a quiescent hub and apply lifecycle bookkeeping; see FrameState.
  void sweep_();
  // The selection function: best READY entry (ordered by priority; FIFO by seq within each group), or nullptr.
  ModbusDeviceCommand *select_next_ready_();
  // Locate the single entry waiting for a response (WAITING/INTERRUPTED/WAITING_RETIRED/INTERRUPTED_RETIRED).
  ModbusDeviceCommand *find_waiting_();
  // End the wait for a response on send-wait timeout (the loop() watchdog body); see FrameState.
  void expire_waiting_();

  uint32_t send_wait_time_us_{2000000};
  uint32_t turnaround_delay_us_{0};

  // Set on transmit, cleared on the transaction-ending transition; send_next_frame_ won't select
  // while it is set, so at most one frame is awaiting a response.
  bool waiting_for_response_{false};

  // Set whenever a transition leaves owed callbacks behind; quiet loop() passes skip the sweep.
  bool sweep_needed_{false};
  // Monotonic stamp source for ModbusDeviceCommand::seq.
  uint16_t next_seq_{0};

  // Plain append-order container; ordering lives in select_next_ready_(), lifecycle in FrameState.
  std::deque<ModbusDeviceCommand> tx_buffer_;
};

// Transaction status: std::nullopt on success, otherwise a Modbus exception code
using ResponseStatus = std::optional<ExceptionCode>;

/// True when a transaction carried no exception.
inline bool succeeded(ResponseStatus status) { return !status.has_value(); }

// Register values exchanged with server handlers, in address order. Sized at the larger of the two protocol
// maxima (read = 125 / 0x7D, write = 123 / 0x7B); the per-direction count limit is enforced by the hub, not by
// the capacity of this type.
using RegisterValues = StaticVector<uint16_t, MAX_NUM_OF_REGISTERS_TO_READ>;

class ModbusServerHub : public Modbus {
 public:
  ModbusServerHub() = default;
  void dump_config() override;
  void register_device(ModbusServerDevice *device) { this->devices_.push_back(device); }

 protected:
  void parse_modbus_frames() override;
  bool parse_modbus_client_frame_();
  void process_modbus_server_frame(uint8_t address, std::span<const uint8_t> pdu) override;
  void process_modbus_client_frame_(uint8_t address, uint8_t function_code, std::span<const uint8_t> data);
  // Dispatches a broadcast (address 0) write to every registered device; broadcasts are never answered.
  void process_broadcast_frame_(uint8_t function_code, std::span<const uint8_t> data);
  // Parses a WRITE_SINGLE_REGISTER / WRITE_MULTIPLE_REGISTERS PDU into start_address and the address order register
  // values, validating the register count and address range. Shared by unicast and broadcast writes.
  ResponseStatus parse_write_single_(std::span<const uint8_t> data, uint16_t &start_address, RegisterValues &registers);
  ResponseStatus parse_write_multiple_(std::span<const uint8_t> data, uint16_t &start_address,
                                       RegisterValues &registers);
  // Assembles host-order registers from the big-endian bytes in values and appends them to registers.
  void assemble_registers_(std::span<const uint8_t> values, RegisterValues &registers);
  ModbusServerDevice *find_device_(uint8_t address);
  // Returns std::nullopt if [start_address, start_address + count) fits in a 16-bit address space, otherwise
  // ILLEGAL_DATA_ADDRESS. The caller sends the exception reply if one is required. Shared by the
  // register/coil/discrete-input handlers, which all use a 16-bit address space.
  ResponseStatus check_address_range_(uint16_t start_address, uint16_t count);

  // Parses read request data. max_entities is the protocol ceiling for the function code; entity_name labels
  // the rejection log.
  ResponseStatus parse_read_request_(std::span<const uint8_t> data, uint16_t max_entities, const LogString *entity_name,
                                     uint16_t &start_address, uint16_t &count);

  // Parses single-coil write data
  ResponseStatus parse_write_single_coil_(std::span<const uint8_t> data, uint16_t &start_address, bool &value);

  // Parses write-multiple-coil data into a packed-bit view pointing straight into the receive buffer, so the
  // coil values are never copied.
  ResponseStatus parse_write_multiple_coils_(std::span<const uint8_t> data, uint16_t &start_address, uint16_t &count,
                                             std::span<const uint8_t> &packed_bytes);

  // Builds the body of a register read response into response_buffer. Returns false once an exception has
  // been sent: the one the handler reported via status, or SERVICE_DEVICE_FAILURE if it returned the wrong
  // number of registers, the count exceeds the protocol read limit, or the body does not fit.
  bool build_or_reject_read_response_(uint8_t address, uint8_t function_code, ResponseStatus status,
                                      uint16_t number_of_registers, const RegisterValues &registers,
                                      std::span<uint8_t> response_buffer, uint16_t &response_len);
  void send_raw_(const uint8_t *payload, uint16_t len);
  // Sends and logs the exception reply when status holds one; returns true if the request was rejected.
  bool rejected_(uint8_t address, uint8_t function_code, ResponseStatus status);
  void send_exception_(uint8_t address, uint8_t function_code, ExceptionCode exception_code);
  void send_response_(uint8_t address, uint8_t function_code, const uint8_t *payload, uint16_t payload_len);
  uint8_t expecting_peer_response_{0};
  std::vector<ModbusServerDevice *> devices_;

  // Holds the raw payload of a single reply deferred for sending when tx was blocked at send time.
  // Only one server reply can be waiting at once, so a single fixed buffer avoids heap allocation.
  std::array<uint8_t, MAX_RAW_SIZE> deferred_payload_;
  uint16_t deferred_payload_len_{0};
};

/// Callback contract. Each accepted request ends in exactly ONE terminal: on_response() (data),
/// on_error() (exception), on_no_response() (timeout/interruption), or on_not_sent() (dropped by
/// clear_tx_queue_for_address before transmission). A request refused at queue_pdu() (false return)
/// gets none, and a broadcast (address 0) gets on_sent() with NO terminal, since a broadcast is never
/// answered (Modbus 4.1). on_sent() is additional, once per transmission, never for an on_not_sent()
/// request. on_response()/on_error() fire at parse time and on_no_response() at the send-wait watchdog,
/// all from a quiescent hub; only on_not_sent() is delivered by the sweep. Sending or clearing from
/// inside a callback is safe (picked up by the next sweep). Exceptions to "exactly one terminal":
/// a broadcast is fire-and-forget (on_sent, no terminal); clear_tx_queue_for_device() drops the caller's
/// own frames silently; a continuous poll's cycles are its own accounting (a one-shot duplicate
/// downgrades the poll to a one-shot; a continuous duplicate merges into it).
///
/// Invariants:
/// - Public entry points (queue_pdu/clear_tx_queue_*) only append to the queue or mutate an existing
///   entry through its callback-free transition methods.
/// - Public entry points can never trigger a callback synchronously.
/// - Callbacks are delivered only from within loop().
/// - At most one callback is ever issued between calls to sweep_():
///     sweep_ -> parse (response OR error) OR timeout (no_response) -> sweep_ -> send (sent) -> sweep_ (next loop)
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
  /// Called when an accepted request was dropped before transmission by clear_tx_queue_for_address().
  /// (on_modbus_* below are deprecated pre-rename spellings; the defaults forward during deprecation.)
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
  ESPDEPRECATED("Use the typed read_*/write_* helpers or queue_pdu() instead. Removed in 2027.2.0", "2026.8.0")
  void send(uint8_t function, uint16_t start_address, uint16_t number_of_entities, uint8_t payload_len = 0,
            const uint8_t *payload = nullptr) {
    this->parent_->queue_pdu(
        this->address_,
        helpers::create_client_pdu((FunctionCode) function, start_address, number_of_entities, payload, payload_len),
        this);
  }
  /// See ModbusClientHub::queue_pdu() for the return contract.
  bool queue_pdu(std::span<const uint8_t> pdu, CommandOptions options = {}) {
    return this->parent_->queue_pdu(this->address_, pdu, this, options);
  }
  // Remove before 2027.2.0. As on the hub, this is the signature 2026.7.4 shipped: void, no options.
  ESPDEPRECATED("Use queue_pdu() instead - the call queues a request, it does not send one, and it "
                "reports whether the request was accepted. Removed in 2027.2.0",
                "2026.8.0")
  void send_pdu(std::span<const uint8_t> pdu) { this->queue_pdu(pdu); }
  ESPDEPRECATED("Use queue_pdu() instead (the device address is prepended for you). Removed in 2027.2.0", "2026.8.0")
  void send_raw(const std::vector<uint8_t> &payload) {
    if (payload.empty())
      return;  // too short to contain a PDU; refused at the door like any invalid send
    this->parent_->queue_pdu(payload[0], std::span<const uint8_t>(payload).subspan(1), this);
  }
  // The typed request builders below all queue through queue_pdu() and share its return contract.
  // Reads use the table-appropriate function code; an unreadable entity type maps to INVALID, which
  // create_read_pdu() rejects into an empty PDU and queue_pdu() refuses with a false return.
  bool read_entities(EntityType entity_type, uint16_t start_address, uint16_t number_of_entities,
                     CommandOptions options = {}) {
    return this->queue_pdu(helpers::create_read_pdu(helpers::modbus_register_read_function(entity_type), start_address,
                                                    number_of_entities),
                           options);
  }
  bool read_input_registers(uint16_t start_address, uint16_t number_of_registers, CommandOptions options = {}) {
    return this->queue_pdu(
        helpers::create_read_pdu(FunctionCode::READ_INPUT_REGISTERS, start_address, number_of_registers), options);
  }
  bool read_holding_registers(uint16_t start_address, uint16_t number_of_registers, CommandOptions options = {}) {
    return this->queue_pdu(
        helpers::create_read_pdu(FunctionCode::READ_HOLDING_REGISTERS, start_address, number_of_registers), options);
  }
  bool read_coils(uint16_t start_address, uint16_t number_of_coils, CommandOptions options = {}) {
    return this->queue_pdu(helpers::create_read_pdu(FunctionCode::READ_COILS, start_address, number_of_coils), options);
  }
  bool read_discrete_inputs(uint16_t start_address, uint16_t number_of_inputs, CommandOptions options = {}) {
    return this->queue_pdu(
        helpers::create_read_pdu(FunctionCode::READ_DISCRETE_INPUTS, start_address, number_of_inputs), options);
  }
  bool write_single_register(uint16_t start_address, uint16_t value) {
    return this->queue_pdu(helpers::create_write_single_register_pdu(start_address, value));
  }
  bool write_single_coil(uint16_t address, bool value) {
    return this->queue_pdu(helpers::create_write_single_coil_pdu(address, value));
  }
  bool write_multiple_registers(uint16_t start_address, std::span<const uint16_t> values) {
    // Empty goes to the full-size builder so the rejection log names this method's limit, not the small one's.
    if (!values.empty() && values.size() <= helpers::MAX_FEW_REGISTERS)
      return this->queue_pdu(helpers::create_write_few_registers_pdu(start_address, values));
    return this->queue_pdu(helpers::create_write_registers_pdu(start_address, values));
  }
  /// Note: std::vector<bool> cannot bind to std::span<const bool>; use a contiguous bool container or the packed
  /// overload.
  bool write_multiple_coils(uint16_t start_address, std::span<const bool> values) {
    return this->queue_pdu(helpers::create_write_coils_pdu(start_address, values));
  }
  /// Packed variant: a PackedBits view (the same layout on_read_coils() delivers), so
  /// read-modify-write needs no unpack/repack.
  bool write_multiple_coils(uint16_t start_address, PackedBits bits) {
    return this->queue_pdu(helpers::create_write_coils_pdu(start_address, bits));
  }
  /// FC 0x17: the read-back is delivered through on_read_holding_registers(), and a device exception
  /// (typically a rejected write half) arrives there too via its status - one callback handles both
  /// outcomes with no on_error() override needed.
  bool read_write_multiple_registers(uint16_t read_start_address, uint16_t read_count, uint16_t write_start_address,
                                     std::span<const uint16_t> write_values) {
    return this->queue_pdu(helpers::create_read_write_multiple_registers_pdu(read_start_address, read_count,
                                                                             write_start_address, write_values));
  }
  inline void clear_tx_queue_for_address() { this->parent_->clear_tx_queue_for_address(this->address_); }
  inline void clear_tx_queue_for_device() { this->parent_->clear_tx_queue_for_device(this); }

  bool ready_for_immediate_send() { return this->parent_->tx_buffer_empty() && !this->parent_->tx_blocked(); }

 protected:
  /// Parses the request/response PDU pair and dispatches to the matching high-level typed callback
  void dispatch_response_(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu,
                          ResponseStatus status);

  ModbusClientHub *parent_{nullptr};
  uint8_t address_{0};
  bool custom_response_warned_{false};  // first unhandled custom response warns; repeats log at VERBOSE
};

// Compatibility shim adapting the span-based hooks back to the pre-2026.8 on_modbus_data()/
// on_modbus_error() signatures (the owning-vector heap copy exists only on this deprecated path).
// Remove before 2027.2.0 (window restarted when the plain alias became a behavior shim in 2026.8.0).
class ESPDEPRECATED("Subclass ModbusClientDevice and override on_response()/on_error() instead. Removed in 2027.2.0",
                    "2026.8.0") ModbusDevice : public ModbusClientDevice {
 public:
  using ModbusClientDevice::ModbusClientDevice;
  virtual void on_modbus_data(const std::vector<uint8_t> &data) {}
  virtual void on_modbus_error(uint8_t function_code, uint8_t exception_code) {}

  void on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) override {
    // Custom (user-defined) function codes historically delivered the payload starting AT the function
    // code byte (frame data_offset 1). server_pdu_payload() drops that byte, so pass the whole PDU for
    // them - external components match the first byte against the code they sent (issue #17994).
    auto payload = !response_pdu.empty() && helpers::is_function_code_custom(response_pdu[0])
                       ? response_pdu
                       : helpers::server_pdu_payload(response_pdu);
    this->on_modbus_data(std::vector<uint8_t>(payload.begin(), payload.end()));
  }
  void on_error(std::span<const uint8_t> request_pdu, ExceptionCode exception_code) override {
    this->on_modbus_error(request_pdu.empty() ? 0 : request_pdu[0], static_cast<uint8_t>(exception_code));
  }
};

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
  /// Coil/discrete-input reads: set the requested bits (bit 0 = the coil at start_address) with
  /// bits.set(). The view covers bits.size() pre-zeroed bits and writes land directly in the hub's
  /// response buffer (no copy); it is only valid during the call.
  virtual ResponseStatus on_read_bits(uint16_t start_address, MutablePackedBits bits) {
    return ExceptionCode::ILLEGAL_FUNCTION;
  };
  virtual ResponseStatus on_read_coils(uint16_t start_address, MutablePackedBits bits) {
    return this->on_read_bits(start_address, bits);
  };
  virtual ResponseStatus on_read_discrete_inputs(uint16_t start_address, MutablePackedBits bits) {
    return this->on_read_bits(start_address, bits);
  };
  /// Coil writes deliver the values as a PackedBits view over the hub's receive buffer (only valid
  /// during the call). A single-coil write (FC 0x05) arrives as bits.size() == 1.
  virtual ResponseStatus on_write_coils(uint16_t start_address, PackedBits bits) {
    return ExceptionCode::ILLEGAL_FUNCTION;
  };

 protected:
  uint8_t address_{0};
};

}  // namespace esphome::modbus

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
  // Transmit a frame. Callers gate on tx_blocked() first, but the pre-send delay can span several ms,
  // so this re-checks after the delay and returns false without transmitting if a byte arrived in that
  // window (the caller then leaves its entry to retry). Returns true once the frame has been transmitted.
  bool send_frame_(const ModbusFrame &frame);
  // Scans forward from min_length to find a frame boundary by CRC match for custom function codes.
  // Returns the matched frame length, or 0 if no valid CRC was found within MAX_FRAME_SIZE.
  uint16_t find_frame_end_by_crc_(uint16_t min_length) const;

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

// Transmit ordering, highest first: writes before one-shot reads before continuous polls. Derived
// at selection time, never caller-chosen or stored.
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
  WAITING_RETIRED,      // cleared while WAITING: a late response is still delivered as its usual terminal
  INTERRUPTED_RETIRED,  // cleared while INTERRUPTED: still distrusts late frames, ends in on_no_response
  RETIRED,              // cleared, off the wire
};

// Per-command send options. Append-only; pass via designated initializers ({.continuous = true}).
struct CommandOptions {
  // A continuous poll lives in the queue until cancelled or failed; ignored for mutating codes.
  bool continuous{false};
};

struct ModbusDeviceCommand {
  ModbusClientDevice *device;
  ModbusFrame frame;
  FrameState state{FrameState::READY};
  // A continuous poll is a subscription: pending fixed at 1, removed only by cancellation or failure.
  bool continuous{false};
  // Accepted requests this entry stands for, capped at max_pending(); drains one terminal each.
  uint8_t pending{1};
  // Place-in-line stamp (hub's free-running counter); selection takes the oldest for round-robin
  // fairness within a class. Meant to wrap.
  uint16_t seq{0};

  // Build a command from a PDU span (caller bounds it to MAX_PDU_SIZE); fully initialized here.
  ModbusDeviceCommand(ModbusClientDevice *device, uint8_t address, std::span<const uint8_t> pdu,
                      bool continuous = false, uint16_t seq = 0)
      : device(device),
        frame(address, pdu.data(), static_cast<uint16_t>(pdu.size())),
        continuous(continuous),
        seq(seq) {}

  // Transmit ordering class, derived (never stored): a continuous poll ranks below every one-shot.
  CommandPriority priority() const {
    return this->continuous ? CommandPriority::CONTINUOUS : classify(this->frame.pdu()[0]);
  }
  // Wire-derived class: mutating codes rank WRITE; exception-flagged codes are excluded.
  static CommandPriority classify(uint8_t function_code) {
    if (helpers::is_function_code_exception(function_code))
      return CommandPriority::READ;
    if (helpers::is_function_code_write(function_code)) {
      return CommandPriority::WRITE;
    }
    return CommandPriority::READ;
  }

  // Requests this entry can serve: a standard read twice (run plus one re-run), everything else once.
  uint8_t max_pending() const {
    const uint8_t fc = this->frame.pdu()[0];
    const bool requeueable = !helpers::is_function_code_exception(fc) && helpers::is_function_code_read_only(fc);
    return (requeueable && !this->continuous) ? 2 : 1;
  }
  // Device-scoped clear: detach with no callback (device-less, pending 0). An entry still waiting for
  // a response keeps its state as a reply-ignoring shell that resolves silently; any other goes RETIRED.
  void silent_retire() {
    if (!this->waiting_state())
      this->state = FrameState::RETIRED;
    this->pending = 0;
    this->device = nullptr;
  }
  // Fire-and-forget completion for a broadcast (address 0): the frame was transmitted (on_sent already
  // fired), but a broadcast is never answered (Modbus 4.1), so the entry retires with NO terminal
  // callback and the sweep erases it. Unlike response()/error()/timed_out(), it delivers nothing.
  // A broadcast only carries a write or a custom code (reads are refused at queue_pdu()), and every such
  // code caps pending at 1, so pending is always 1 here - clear it.
  void complete_broadcast() {
    this->state = FrameState::RETIRED;
    this->pending = 0;
  }
  // Re-ready for another transmission, restamped to the tail of its class (hub passes next_seq_++).
  void requeue(uint16_t seq) {
    this->state = FrameState::READY;
    this->seq = seq;
  }
  // Re-task a frame that lives on: upgrade a one-shot to a continuous poll, or downgrade a poll back to
  // a one-shot. Either way the entry keeps running and owes a request, so this is not a plain setter -
  // to tear an entry down instead, use retire()/silent_retire(), which leave pending as the count owed.
  // On: the entry becomes a continuous poll, superseding any absorbed requests (pending resets to the
  // single subscription). Off: a one-shot duplicate has cancelled the poll, but the entry must still run
  // once to serve that request - so restore one first. While the flag is still set max_pending() is 1,
  // so the restore lifts a terminated poll (pending 0, after an error/timeout) back to 1 and is a no-op
  // on a live poll already at 1; the flag drops afterwards, when a read's cap can widen to 2 without
  // retroactively inflating that no-op.
  void make_continuous(bool continuous) {
    if (continuous) {
      this->continuous = true;
      this->pending = 1;
    } else {
      this->increment_pending();
      this->continuous = false;
    }
  }
  // Address-scoped clear: keep pending and device so the sweep delivers one on_not_sent() per un-run
  // request. An entry still waiting for a response keeps its in-flight request (whose usual terminal is
  // still coming) and drains only its duplicates: WAITING -> WAITING_RETIRED, and INTERRUPTED ->
  // INTERRUPTED_RETIRED which keeps distrusting late frames (they were already interrupted). Any other
  // state -> RETIRED, draining everything. A cleared frame that then times out still honors a retry:
  // the clear is address-scoped (any device may call it) while the retry is the owning device's call
  // via on_no_response - the bus obeys the owner.
  void retire() {
    if (this->state == FrameState::WAITING) {
      this->state = FrameState::WAITING_RETIRED;
    } else if (this->state == FrameState::INTERRUPTED) {
      this->state = FrameState::INTERRUPTED_RETIRED;
    } else if (!this->waiting_state()) {  // an already-retired shell stays put; off the wire -> RETIRED
      this->state = FrameState::RETIRED;
    }
    this->continuous = false;
  }

  // True while the entry is still waiting for a response; the erase pass exempts these even at pending 0.
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
  // Add one request, honouring the cap; false = already at cap (absorb a duplicate, restore a retry).
  bool increment_pending() {
    if (this->pending < this->max_pending()) {
      this->pending++;
      return true;
    }
    return false;
  }

  // Terminal/lifecycle methods: each owns its transition, callback, and pending accounting and
  // returns whether a callback ran. Out-of-line: ModbusClientDevice is incomplete here.
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
  void set_send_wait_time(uint16_t time_in_ms) { this->send_wait_time_ = time_in_ms; }
  void set_turnaround_time(uint16_t time_in_ms) { this->turnaround_delay_ms_ = time_in_ms; }
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
  /// Queue a request. The name says queue, not send: the frame is appended to the transmit queue and
  /// goes out later from loop(), so a true return means accepted into the machine (it will resolve in
  /// exactly one terminal callback - except a broadcast (address 0), which is never answered and so gets
  /// only on_sent()), NOT that anything reached the wire - that is on_sent(). False means
  /// it never entered the machine at all (empty or oversize PDU, full queue, anonymous or over-cap
  /// duplicate) and no callback of any kind will follow; the false return is the whole story.
  bool queue_pdu(uint8_t address, std::span<const uint8_t> pdu, ModbusClientDevice *device = nullptr,
                 CommandOptions options = {});
  // Remove before 2027.2.0. Deliberately the signature 2026.7.4 shipped - void, and no CommandOptions:
  // the bool return and the options argument arrived after that release, so nothing external can be
  // relying on them under this name. Callers who want the queued/refused answer move to queue_pdu().
  ESPDEPRECATED("Use queue_pdu() instead - the call queues a request, it does not send one, and it "
                "reports whether the request was accepted. Removed in 2027.2.0",
                "2026.8.0")
  void send_pdu(uint8_t address, std::span<const uint8_t> pdu, ModbusClientDevice *device = nullptr) {
    this->queue_pdu(address, pdu, device);
  }
  ESPDEPRECATED("Use queue_pdu(payload[0], <pdu bytes>, device) instead. Removed in 2027.2.0", "2026.8.0")
  void send_raw(const std::vector<uint8_t> &payload, ModbusClientDevice *device = nullptr);
  // Clear an address's commands; each un-run request resolves via on_not_sent(), but a frame on the
  // wire still runs to its usual terminal. clear_tx_queue_for_device() instead discards silently.
  void clear_tx_queue_for_address(uint8_t address);
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
  // Locate the single entry waiting for a response (WAITING/INTERRUPTED/WAITING_RETIRED/INTERRUPTED_RETIRED).
  ModbusDeviceCommand *find_waiting_();
  // End the wait for a response on send-wait timeout (the loop() watchdog body); see FrameState.
  void expire_waiting_();

  uint16_t send_wait_time_{2000};
  uint16_t turnaround_delay_ms_{0};

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

/// True when a transaction carried no exception. The optional holds the exception, so has_value() means
/// the request FAILED - the inverse of how "status" usually reads. Prefer this at the call site; the
/// bare !status.has_value() has already been mistaken for a failure check more than once. Where the code
/// is going to unwrap the exception anyway, status.has_value() followed by status.value() stays clearer.
inline bool succeeded(ResponseStatus status) { return !status.has_value(); }

// Register values exchanged with server handlers, in host byte order. Sized at the larger of the two protocol
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
  // Parses a WRITE_SINGLE_REGISTER / WRITE_MULTIPLE_REGISTERS PDU into start_address and the host-order register
  // values, validating the register count and address range. Returns std::nullopt on success, otherwise the Modbus
  // exception code describing the failure. Shared by unicast writes (which reply with the exception) and broadcast
  // writes (which silently drop invalid frames).
  ResponseStatus parse_write_single_(std::span<const uint8_t> data, uint16_t &start_address, RegisterValues &registers);
  ResponseStatus parse_write_multiple_(std::span<const uint8_t> data, uint16_t &start_address,
                                       RegisterValues &registers);
  // Appends the big-endian register values in values to registers, in host byte order.
  void assemble_registers_(std::span<const uint8_t> values, RegisterValues &registers);
  ModbusServerDevice *find_device_(uint8_t address);
  // Returns std::nullopt if [start_address, start_address + count) fits in the 16-bit address space,
  // otherwise ILLEGAL_DATA_ADDRESS. The caller sends the exception reply if one is required - a broadcast
  // write is never answered, so the check cannot send it itself. Shared by the register and
  // coil/discrete-input handlers, which all address the same 16-bit space.
  ResponseStatus check_address_range_(uint16_t start_address, uint16_t count);

  // Parses a read request PDU (start address(2) + quantity(2)), shared by the register and
  // coil/discrete-input reads so the two cannot drift apart. max_entities is the protocol ceiling for the
  // function code; entity_name only labels the rejection log.
  ResponseStatus parse_read_request_(std::span<const uint8_t> data, uint16_t max_entities, const LogString *entity_name,
                                     uint16_t &start_address, uint16_t &count);

  // Parses a single-coil write PDU (FC 0x05), which carries a 2-byte on/off value rather than packed
  // bytes. The caller packs value into a byte it owns to build the PackedBits view the handlers take.
  ResponseStatus parse_write_single_coil_(std::span<const uint8_t> data, uint16_t &start_address, bool &value);

  // Parses a multiple-coil write PDU (FC 0x0F) into a packed-bit view pointing straight into the receive
  // buffer, so the coil values are never copied. Both coil parsers are shared by the addressed and
  // broadcast paths so the two validate identically.
  ResponseStatus parse_write_multiple_coils_(std::span<const uint8_t> data, uint16_t &start_address, uint16_t &count,
                                             std::span<const uint8_t> &packed_bytes);

  // Builds the body of a register read response (byte count followed by the big-endian register values) into
  // response_buffer. Shared by every function code that answers with register values, so the read reply stays
  // identical across them. Returns false once an exception has been sent: the one the handler reported via
  // status, or SERVICE_DEVICE_FAILURE if it returned the wrong number of registers, the count exceeds the
  // protocol read limit, or the body does not fit.
  bool build_or_reject_read_response_(uint8_t address, uint8_t function_code, ResponseStatus status,
                                      uint16_t number_of_registers, const RegisterValues &registers,
                                      std::span<uint8_t> response_buffer, uint16_t &response_len);
  void send_raw_(const uint8_t *payload, uint16_t len);
  // Sends and logs the exception reply when status holds one; returns true if the request was rejected.
  // Every parse and handler rejection funnels through here, so the reply and its log cannot drift apart.
  bool rejected_(uint8_t address, uint8_t function_code, ResponseStatus status);
  void send_exception_(uint8_t address, uint8_t function_code, ExceptionCode exception_code);
  void send_response_(uint8_t address, uint8_t function_code, const uint8_t *payload, uint16_t payload_len);
  uint8_t expecting_peer_response_{0};
  std::vector<ModbusServerDevice *> devices_;

  // Stamp of the last "broadcast reached no device" warning, 0 until the first one is logged. Rate limiting
  // on time rather than on address keeps the log bounded no matter how many addresses a shared bus carries.
  uint32_t last_unaccepted_broadcast_warn_{0};

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
  /// See ModbusClientHub::queue_pdu(): true = accepted into the queue and a terminal callback will
  /// follow (except a broadcast (address 0), which is never answered and so gets only on_sent()),
  /// false = refused at the door and nothing further happens. Neither means the frame is on the wire;
  /// on_sent() reports that.
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
  // The typed request builders below all queue through queue_pdu(), so they share its contract: true
  // means the request is queued and will resolve in exactly one terminal callback (except a broadcast
  // (address 0), which is never answered and so gets only on_sent()), false means it was refused outright
  // with no callback. Neither says the frame has been transmitted - on_sent() does.
  // Reads via the table-appropriate function code; an unreadable entity type maps to INVALID, which
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
  /// FC 0x17: the read-back is delivered through on_read_holding_registers() (the response carries only the
  /// read registers, the same wire shape as a holding-register read). A device exception - typically a
  /// rejected write half - arrives at that same on_read_holding_registers() with the error in its status,
  /// exactly as success does, so a subclass overriding that one callback handles both outcomes and never
  /// needs to also override on_error().
  bool read_write_multiple_registers(uint16_t read_start_address, uint16_t read_count, uint16_t write_start_address,
                                     std::span<const uint16_t> write_values) {
    return this->queue_pdu(helpers::create_read_write_multiple_registers_pdu(read_start_address, read_count,
                                                                             write_start_address, write_values));
  }
  inline void clear_tx_queue_for_address() { this->parent_->clear_tx_queue_for_address(this->address_); }
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

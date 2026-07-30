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

static constexpr uint16_t MODBUS_TX_BUFFER_SIZE = 15;
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

struct ModbusDeviceCommand {
  ModbusClientDevice *device;
  ModbusFrame frame;
  bool interrupted{false};
  /// Marked by clear_tx_queue_for_address() before it starts notifying, so frames re-queued by an
  /// on_not_sent() callback (which are unmarked) are never swept by the clear that triggered them.
  bool marked_for_deletion{false};

  ModbusDeviceCommand(ModbusClientDevice *device, uint8_t address, const uint8_t *src, uint16_t len)
      : device(device), frame(address, src, len) {}
  /// Build a command from a PDU span: a caller-supplied PDU, or an existing frame's own pdu() when re-queueing
  /// Callers must bound the PDU to MAX_PDU_SIZE
  ModbusDeviceCommand(ModbusClientDevice *device, uint8_t address, std::span<const uint8_t> pdu)
      : device(device), frame(address, pdu.data(), static_cast<uint16_t>(pdu.size())) {}
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
  void send_pdu(uint8_t address, std::span<const uint8_t> pdu, ModbusClientDevice *device = nullptr);
  ESPDEPRECATED("Use send_pdu(payload[0], <pdu bytes>, device) instead. Removed in 2027.2.0", "2026.8.0")
  void send_raw(const std::vector<uint8_t> &payload, ModbusClientDevice *device = nullptr);
  // Drop the queued commands for an address; every dropped frame resolves via its owner's on_not_sent(),
  // so other devices sharing the address observe the drop. The in-flight frame is only detached (silently)
  // when clear_sent is set. clear_tx_queue_for_device() SILENTLY discards the caller's own frames
  // (supersede/teardown semantics); see the lifecycle note on ModbusClientDevice.
  void clear_tx_queue_for_address(uint8_t address, bool clear_sent = true);
  void clear_tx_queue_for_device(ModbusClientDevice *device);

 protected:
  int32_t tx_delay_remaining() override;
  void parse_modbus_frames() override;
  void process_modbus_server_frame(uint8_t address, std::span<const uint8_t> pdu) override;
  void send_next_frame_();
  // Notify the waiting device of no response; re-queues the frame if on_no_response() returns true.
  // wfr is the caller's checked reference to waiting_for_response_.
  void notify_no_response_(ModbusDeviceCommand &wfr);
  void requeue_waiting_frame_(ModbusDeviceCommand &wfr);

  uint16_t send_wait_time_{2000};
  uint16_t turnaround_delay_ms_{0};
  std::optional<ModbusDeviceCommand> waiting_for_response_;

  // std::deque is appropriate here since we need a FIFO buffer, and we can't know ahead of time how many
  // requests will be queued. Each modbus component may queue multiple requests, and the sequence of scheduling
  // may change at run time.
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
  // Only one server reply can be in flight at once, so a single fixed buffer avoids heap allocation.
  std::array<uint8_t, MAX_RAW_SIZE> deferred_payload_;
  uint16_t deferred_payload_len_{0};
};

// Transaction status: std::nullopt on success, otherwise a Modbus exception code
using ResponseStatus = std::optional<ExceptionCode>;

/// Command lifecycle: each accepted command (a send_pdu()/typed-helper call, or a hub re-queue from
/// a retry) ends in exactly ONE terminal callback: on_response() (valid response), on_error()
/// (exception response), on_no_response() (timeout or interrupted transaction), or on_not_sent()
/// (never transmitted: send failure or full queue). on_sent() is additional, not
/// terminal: it fires once per wire transmission, before whichever of data/error/no_response follows,
/// and never for a command that ends in on_not_sent().
/// The exceptions to "exactly one terminal":
///  - clear_tx_queue_for_device() drops the caller's OWN queued commands SILENTLY (supersede/teardown
///    semantics), and both clear variants detach the in-flight frame silently.
///    clear_tx_queue_for_address() DOES resolve every queued frame it drops via the owner's
///    on_not_sent() (delivered one at a time, after that frame leaves the queue).
///  - while a device's own on_not_sent() is on the stack, further on_not_sent() deliveries to THAT
///    device are dropped (see trigger_not_sent()). In particular, a clear issued from inside your own
///    on_not_sent() resolves your remaining frames silently - treat it like
///    clear_tx_queue_for_device(): you cleared them, you know. Other owners are still notified.
/// Sending from inside on_not_sent() is hazardous: the notification may itself mean the queue is full
/// or refusing, and this device's retry that is refused again is dropped WITHOUT a callback (the
/// guard above, which bounds what would otherwise be unbounded re-entry) - prefer re-sending from a
/// later trigger or the component's update()/loop().
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
  /// Called when no request could be sent (e.g. queue full, transmission blocked).
  /// Do not attempt to queue a command in this callback.
  /// (The on_modbus_* names below are deprecated pre-rename spellings; the defaults forward so
  /// external devices overriding them keep working through the deprecation window.)
  virtual void on_not_sent(std::span<const uint8_t> request_pdu) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    this->on_modbus_not_sent();
#pragma GCC diagnostic pop
  }
  /// Non-virtual entry point the hub uses for EVERY on_not_sent() delivery (refusals and clear-queue
  /// sweeps alike). While this device's on_not_sent() is on the stack, further deliveries to it are
  /// dropped: this bounds every send->refuse and clear->sweep recursion, including cycles through
  /// multiple devices (each device can appear on the stack at most once). The documented cost: a clear
  /// issued from inside your own on_not_sent() resolves your remaining frames SILENTLY, while other
  /// owners are still notified (their guards are not set) - see the lifecycle contract above.
  void trigger_not_sent(std::span<const uint8_t> request_pdu) {
    if (this->notifying_not_sent_)
      return;
    this->notifying_not_sent_ = true;
    this->on_not_sent(request_pdu);
    this->notifying_not_sent_ = false;
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
  void send_pdu(std::span<const uint8_t> pdu) { this->parent_->send_pdu(this->address_, pdu, this); }
  ESPDEPRECATED("Use send_pdu() instead (the device address is prepended for you). Removed in 2027.2.0", "2026.8.0")
  void send_raw(const std::vector<uint8_t> &payload) {
    if (payload.empty()) {
      // Through the guard like every other delivery, so a handler calling send_raw({}) cannot recurse.
      this->trigger_not_sent({});
      return;
    }
    this->parent_->send_pdu(payload[0], std::span<const uint8_t>(payload).subspan(1), this);
  }
  // Reads via the table-appropriate function code; an unreadable entity type maps to INVALID, which
  // create_read_pdu() rejects into an empty PDU and send_pdu() signals via on_not_sent().
  void read_entities(EntityType entity_type, uint16_t start_address, uint16_t number_of_entities) {
    this->send_pdu(helpers::create_read_pdu(helpers::modbus_register_read_function(entity_type), start_address,
                                            number_of_entities));
  }
  void read_input_registers(uint16_t start_address, uint16_t number_of_registers) {
    this->send_pdu(helpers::create_read_pdu(FunctionCode::READ_INPUT_REGISTERS, start_address, number_of_registers));
  }
  void read_holding_registers(uint16_t start_address, uint16_t number_of_registers) {
    this->send_pdu(helpers::create_read_pdu(FunctionCode::READ_HOLDING_REGISTERS, start_address, number_of_registers));
  }
  void read_coils(uint16_t start_address, uint16_t number_of_coils) {
    this->send_pdu(helpers::create_read_pdu(FunctionCode::READ_COILS, start_address, number_of_coils));
  }
  void read_discrete_inputs(uint16_t start_address, uint16_t number_of_inputs) {
    this->send_pdu(helpers::create_read_pdu(FunctionCode::READ_DISCRETE_INPUTS, start_address, number_of_inputs));
  }
  void write_single_register(uint16_t start_address, uint16_t value) {
    this->send_pdu(helpers::create_write_single_register_pdu(start_address, value));
  }
  void write_single_coil(uint16_t address, bool value) {
    this->send_pdu(helpers::create_write_single_coil_pdu(address, value));
  }
  void write_multiple_registers(uint16_t start_address, std::span<const uint16_t> values) {
    this->send_pdu(helpers::create_write_registers_pdu(start_address, values));
  }
  /// Note: std::vector<bool> cannot bind to std::span<const bool>; use a contiguous bool container or the packed
  /// overload.
  void write_multiple_coils(uint16_t start_address, std::span<const bool> values) {
    this->send_pdu(helpers::create_write_coils_pdu(start_address, values));
  }
  /// Packed variant: a PackedBits view (the same layout on_read_coils() delivers), so
  /// read-modify-write needs no unpack/repack.
  void write_multiple_coils(uint16_t start_address, PackedBits bits) {
    this->send_pdu(helpers::create_write_coils_pdu(start_address, bits));
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
  /// True while this device's on_not_sent() is on the stack (see trigger_not_sent()).
  bool notifying_not_sent_{false};
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

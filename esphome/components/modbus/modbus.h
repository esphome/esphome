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

// Transaction status: std::nullopt on success, otherwise the Modbus exception code. Server handlers return it;
// (future) client response callbacks receive it. Named without a side prefix so both directions share it.
using ResponseStatus = std::optional<ExceptionCode>;
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
  void process_modbus_client_frame_(uint8_t address, uint8_t function_code, const uint8_t *data);
  // Dispatches a broadcast (address 0) write to every registered device; broadcasts are never answered.
  void process_broadcast_frame_(uint8_t function_code, const uint8_t *data);
  // Parses a WRITE_SINGLE_REGISTER / WRITE_MULTIPLE_REGISTERS PDU into start_address and the host-order register
  // values, validating the register count and address range. Returns std::nullopt on success, otherwise the Modbus
  // exception code describing the failure. Shared by unicast writes (which reply with the exception) and broadcast
  // writes (which silently drop invalid frames).
  ResponseStatus parse_write_registers_(uint8_t function_code, const uint8_t *data, uint16_t &start_address,
                                        RegisterValues &registers);
  ModbusServerDevice *find_device_(uint8_t address);
  // Returns std::nullopt if [start_address, start_address + number_of_registers) fits in the 16-bit address space,
  // otherwise ILLEGAL_DATA_ADDRESS. The caller sends the exception reply if one is required.
  ResponseStatus check_register_range_(uint16_t start_address, uint16_t number_of_registers);
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
  /// Called with the request PDU this device sent and the response PDU received (both: function code +
  /// data, no address, no CRC). The spans are only valid for the duration of the call - copy the bytes
  /// if they must outlive it. Slice the payload out of the response with helpers::server_pdu_payload().
  virtual void on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) {}
  /// Called with the request PDU and the modbus exception code decoded from the error response.
  virtual void on_error(std::span<const uint8_t> request_pdu, ExceptionCode exception_code) {}
  // The on_modbus_* names are signature-identical renames, so the new defaults forward to the old
  // virtuals: external devices overriding the old names keep working through the deprecation window.
  // Remove the forwards together with the deprecated names.
  virtual void on_not_sent() {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    this->on_modbus_not_sent();
#pragma GCC diagnostic pop
  }
  /// Called when no (valid) response arrived; return true to have the hub re-queue the frame for a retry.
  /// The hub does not bound retries: the device is responsible for limiting them (e.g. track a counter and
  /// return false when exhausted), or an unresponsive peer will starve other traffic on the bus.
  virtual bool on_no_response() {
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
      this->on_not_sent();  // match the hub-level send_raw(): a refused send is always signalled
      return;
    }
    this->parent_->send_pdu(payload[0], std::span<const uint8_t>(payload).subspan(1), this);
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
  ModbusClientHub *parent_{nullptr};
  uint8_t address_{0};
};

// This is for compatibility with external components using the former class name
// Remove before 2026.12.0
using ModbusDevice ESPDEPRECATED("Use ModbusClientDevice instead. Removed in 2026.12.0",
                                 "2026.6.0") = ModbusClientDevice;

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

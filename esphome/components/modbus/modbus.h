#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

#include "esphome/components/modbus/modbus_definitions.h"
#include "esphome/components/modbus/modbus_helpers.h"

#include <array>
#include <cstring>
#include <memory>
#include <vector>
#include <deque>
#include <optional>

namespace esphome::modbus {

static constexpr uint16_t MODBUS_TX_BUFFER_SIZE = 15;
static constexpr uint16_t MODBUS_TX_MAX_DELAY_MS = 5;

struct ModbusFrame {
  // Frame with exact-size allocation to avoid std::vector overhead
  std::unique_ptr<uint8_t[]> data;
  uint16_t size;  // Modbus RTU max is 256 bytes

  ModbusFrame(uint8_t address, const uint8_t *pdu, uint16_t pdu_len)
      : data(std::make_unique<uint8_t[]>(pdu_len + 3)), size(pdu_len + 3) {
    data[0] = address;
    memcpy(data.get() + 1, pdu, pdu_len);
    auto crc = crc16(data.get(), pdu_len + 1);
    data[pdu_len + 1] = crc >> 0;
    data[pdu_len + 2] = crc >> 8;
  }
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
  virtual void process_modbus_server_frame(uint8_t address, uint8_t function_code, const uint8_t *data,
                                           uint16_t len) = 0;
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
                   helpers::create_client_pdu((ModbusFunctionCode) function_code, start_address, number_of_entities,
                                              payload, payload_len),
                   device);
  };
  void send_pdu(uint8_t address, const StaticVector<uint8_t, MAX_PDU_SIZE> &pdu, ModbusClientDevice *device = nullptr) {
    this->queue_raw_(address, pdu.data(), pdu.size(), device);
  }
  void send_raw(const std::vector<uint8_t> &payload, ModbusClientDevice *device = nullptr);
  void clear_tx_queue_for_address(uint8_t address, bool clear_sent = true);
  void clear_tx_queue_for_device(ModbusClientDevice *device);

 protected:
  int32_t tx_delay_remaining() override;
  void parse_modbus_frames() override;
  // Parsers need to handle standard (ModbusFunctionCode) and custom (uint8_t) function codes, so we use uint8_t here.
  void process_modbus_server_frame(uint8_t address, uint8_t function_code, const uint8_t *data, uint16_t len) override;
  void send_next_frame_();
  void queue_raw_(uint8_t address, const uint8_t *pdu, uint16_t pdu_len, ModbusClientDevice *device = nullptr);

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
  // Parsers need to handle standard (ModbusFunctionCode) and custom (uint8_t) function codes, so we use uint8_t here.
  void process_modbus_server_frame(uint8_t address, uint8_t function_code, const uint8_t *data, uint16_t len) override;
  void process_modbus_client_frame_(uint8_t address, uint8_t function_code, const uint8_t *data);
  ModbusServerDevice *find_device_(uint8_t address);
  // Returns true if [start_address, start_address + number_of_registers) fits in the 16-bit address space.
  // On failure, logs and sends an ILLEGAL_DATA_ADDRESS exception to the client.
  bool check_register_range_(uint8_t address, uint8_t function_code, uint16_t start_address,
                             uint16_t number_of_registers);
  void send_raw_(const uint8_t *payload, uint16_t len);
  void send_exception_(uint8_t address, uint8_t function_code, ModbusExceptionCode exception_code);
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
  virtual void on_modbus_data(const std::vector<uint8_t> &data) {}
  virtual void on_modbus_error(uint8_t function_code, uint8_t exception_code) {}
  virtual void on_modbus_not_sent() {}
  virtual void on_modbus_no_response() {}
  void send(uint8_t function, uint16_t start_address, uint16_t number_of_entities, uint8_t payload_len = 0,
            const uint8_t *payload = nullptr) {
    this->parent_->send_pdu(this->address_,
                            helpers::create_client_pdu((ModbusFunctionCode) function, start_address, number_of_entities,
                                                       payload, payload_len),
                            this);
  }
  void send_pdu(const StaticVector<uint8_t, MAX_PDU_SIZE> &pdu) { this->parent_->send_pdu(this->address_, pdu, this); }
  void send_raw(const std::vector<uint8_t> &payload) { this->parent_->send_raw(payload, this); }
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

// Result of a server register handler: std::nullopt means success, otherwise the Modbus exception code to return.
using ServerResponseStatus = std::optional<ModbusExceptionCode>;
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
  virtual ServerResponseStatus on_modbus_read_registers(uint16_t start_address, uint16_t number_of_registers,
                                                        RegisterValues &registers) {
    return ModbusExceptionCode::ILLEGAL_FUNCTION;
  };
  virtual ServerResponseStatus on_modbus_read_input_registers(uint16_t start_address, uint16_t number_of_registers,
                                                              RegisterValues &registers) {
    return this->on_modbus_read_registers(start_address, number_of_registers, registers);
  };
  virtual ServerResponseStatus on_modbus_read_holding_registers(uint16_t start_address, uint16_t number_of_registers,
                                                                RegisterValues &registers) {
    return this->on_modbus_read_registers(start_address, number_of_registers, registers);
  };
  virtual ServerResponseStatus on_modbus_write_registers(uint16_t start_address, const RegisterValues &registers) {
    return ModbusExceptionCode::ILLEGAL_FUNCTION;
  };

 protected:
  uint8_t address_{0};
};

}  // namespace esphome::modbus

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

#include "esphome/components/modbus/modbus_definitions.h"
#include "esphome/components/modbus/modbus_helpers.h"

#include <cstring>
#include <memory>
#include <vector>
#include <deque>
#include <optional>

namespace esphome {
namespace modbus {

static constexpr uint16_t MODBUS_TX_BUFFER_SIZE = 15;
static constexpr uint16_t MODBUS_TX_MAX_DELAY_MS = 5;

struct ModbusFrame {
  // Frame with exact-size allocation to avoid std::vector overhead
  std::unique_ptr<uint8_t[]> data;
  uint16_t size;  // Modbus RTU max is 256 bytes

  ModbusFrame(const uint8_t *src, uint16_t len) : data(std::make_unique<uint8_t[]>(len + 2)), size(len + 2) {
    std::memcpy(this->data.get(), src, len);
    auto crc = crc16(data.get(), len);
    data[len + 0] = crc >> 0;
    data[len + 1] = crc >> 8;
  }

  bool operator==(const ModbusFrame &other) const {
    if (this->size != other.size) {
      return false;
    }
    return std::memcmp(this->data.get(), other.data.get(), this->size) == 0;
  }

  // This is a comparison against a raw payload (without CRC).
  // This is used to check for duplicates in the tx queue without needing to construct full ModbusFrames for every item
  // in the queue.
  bool operator==(const std::vector<uint8_t> &other) const {
    if (this->size - 2 != other.size()) {
      return false;
    }
    return std::memcmp(this->data.get(), other.data(), other.size()) == 0;
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
  int32_t turnaround_delay_remaining_();
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
  uint16_t turnaround_delay_ms_{0};  // This is only used by ModbusClientHub. Servers respond immediately.

  GPIOPin *flow_control_pin_{nullptr};

  std::vector<uint8_t> rx_buffer_;
};

class ModbusClientDevice;
class ModbusServerDevice;

struct ModbusDeviceCommand {
  ModbusClientDevice *device;
  ModbusFrame frame;
  bool interrupted{false};

  ModbusDeviceCommand(ModbusClientDevice *device, const uint8_t *src, uint16_t len) : device(device), frame(src, len) {}
};

class ModbusClientHub : public Modbus {
 public:
  ModbusClientHub() = default;
  void dump_config() override;
  void loop() override;
  void set_send_wait_time(uint16_t time_in_ms) { send_wait_time_ = time_in_ms; }
  void set_turnaround_time(uint16_t time_in_ms) { turnaround_delay_ms_ = time_in_ms; }
  bool tx_buffer_empty();
  bool tx_blocked() override;
  ESPDEPRECATED("Use send_pdu() with create_client_pdu() instead. Removed in 2026.10.0", "2026.4.0")
  void send(uint8_t address, uint8_t function_code, uint16_t start_address, uint16_t number_of_entities,
            uint8_t payload_len = 0, const uint8_t *payload = nullptr, ModbusClientDevice *device = nullptr,
            bool allow_duplicates = false);
  void send_raw(const uint8_t *payload, uint16_t len, ModbusClientDevice *device = nullptr,
                bool allow_duplicates = false);
  ESPDEPRECATED("Use send_raw(const uint8_t *, uint16_t) instead. Removed in 2026.10.0", "2026.4.0")
  void send_raw(const std::vector<uint8_t> &payload, ModbusClientDevice *device = nullptr,
                bool allow_duplicates = false);
  void send_pdu(uint8_t address, const StaticVector<uint8_t, MAX_FRAME_SIZE> &pdu, ModbusClientDevice *device = nullptr,
                bool allow_duplicates = false) {
    uint8_t frame[MAX_FRAME_SIZE];
    frame[0] = address;
    std::memcpy(frame + 1, pdu.data(), pdu.size());
    this->send_raw(frame, static_cast<uint16_t>(1 + pdu.size()), device, allow_duplicates);
  }
  void clear_tx_queue_for_address(uint8_t address, bool clear_sent = true);
  void clear_tx_queue_for_device(ModbusClientDevice *device);

 protected:
  void parse_modbus_frames() override;
  void process_modbus_server_frame(uint8_t address, uint8_t function_code, const uint8_t *data, uint16_t len) override;
  void send_next_frame_();

  uint16_t send_wait_time_{2000};
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
  void send(uint8_t address, uint8_t function_code, const std::vector<uint8_t> &payload);
  void send_raw(const uint8_t *payload, uint16_t len);
  void send_raw(const std::vector<uint8_t> &payload);
  void register_device(ModbusServerDevice *device) { this->devices_.push_back(device); }

 protected:
  void parse_modbus_frames() override;
  bool parse_modbus_client_frame_();
  void process_modbus_server_frame(uint8_t address, uint8_t function_code, const uint8_t *data, uint16_t len) override;
  void process_modbus_client_frame_(uint8_t address, uint8_t function_code, const uint8_t *data, uint16_t len);
  uint8_t expecting_peer_response_{0};
  std::vector<ModbusServerDevice *> devices_;
};

class ModbusClientDevice {
 public:
  ModbusClientDevice() = default;
  ModbusClientDevice(ModbusClientHub *parent, uint8_t address) : parent_(parent), address_(address) {}
  virtual ~ModbusClientDevice() { this->clear_tx_queue_for_device(); }
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
  void send_pdu(const StaticVector<uint8_t, MAX_FRAME_SIZE> &pdu) {
    this->parent_->send_pdu(this->address_, pdu, this);
  }
  void send_raw(const uint8_t *payload, uint16_t len) { this->parent_->send_raw(payload, len, this); }
  ESPDEPRECATED("Use send_pdu(StaticVector<uint8_t, MAX_FRAME_SIZE>) instead. Removed in 2026.10.0", "2026.4.0")
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
class ModbusDevice : public ModbusClientDevice {};

class ModbusServerDevice {
 public:
  ModbusServerDevice() = default;
  ModbusServerDevice(ModbusServerHub *parent, uint8_t address) : parent_(parent), address_(address) {}
  void set_parent(ModbusServerHub *parent) { this->parent_ = parent; }
  void set_address(uint8_t address) { this->address_ = address; }
  virtual void on_modbus_read_registers(uint8_t function_code, uint16_t start_address, uint16_t number_of_registers){};
  virtual void on_modbus_write_registers(uint8_t function_code, const std::vector<uint8_t> &data){};
  void send(uint8_t function, const std::vector<uint8_t> &payload) {
    this->parent_->send(this->address_, function, payload);
  }
  void send_raw(const uint8_t *payload, uint16_t len) { this->parent_->send_raw(payload, len); }
  void send_raw(const std::vector<uint8_t> &payload) { this->parent_->send_raw(payload); }
  void send_error(uint8_t function_code, ModbusExceptionCode exception_code) {
    uint8_t error_response[3] = {this->address_, uint8_t(function_code | FUNCTION_CODE_EXCEPTION_MASK),
                                 static_cast<uint8_t>(exception_code)};
    this->send_raw(error_response, 3);
  }

 protected:
  friend ModbusServerHub;

  ModbusServerHub *parent_{nullptr};
  uint8_t address_{0};
};

}  // namespace modbus
}  // namespace esphome

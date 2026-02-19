#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

#include "esphome/components/modbus/modbus_definitions.h"
#include "esphome/components/modbus/modbus_helpers.h"

#include <vector>
#include <deque>
#include <optional>

namespace esphome {
namespace modbus {

using namespace esphome::modbus::helpers;

static const uint16_t MODBUS_TX_BUFFER_SIZE = 100;
static const uint16_t MODBUS_TX_MAX_DELAY_MS = 5;

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
  virtual void process_modbus_server_frame(uint8_t address, uint8_t function_code,
                                           const std::vector<uint8_t> &data) = 0;
  void clear_rx_buffer_(const std::string &reason, bool warn = false, size_t bytes_to_clear = 0);
  bool send_frame_(const std::vector<uint8_t> &frame);

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

enum class ModbusDeviceCommandPriority : uint8_t { READ_CONTINUOUS, READ_ONCE, READ_AGAIN, WRITE };

struct ModbusDeviceCommand {
  ModbusClientDevice *device;
  std::vector<uint8_t> frame;
  ModbusDeviceCommandPriority priority;
  bool interrupted{false};
  bool operator<(const ModbusDeviceCommand &rhs) { return this->priority < rhs.priority; }
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
  void send_raw(const std::vector<uint8_t> &payload, ModbusClientDevice *device = nullptr, bool continuous = false);
  void clear_tx_queue_for_address(uint8_t address, bool clear_sent = true);
  void clear_tx_queue_for_device(ModbusClientDevice *device);

 protected:
  int32_t send_wait_delay_remaining_();
  void parse_modbus_frames() override;
  void process_modbus_server_frame(uint8_t address, uint8_t function_code, const std::vector<uint8_t> &data) override;
  void send_next_frame_();
  void clear_waiting_for_response_(bool success);

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
  void register_device(ModbusServerDevice *device) { this->devices_.push_back(device); }

 protected:
  void parse_modbus_frames() override;
  bool parse_modbus_client_frame_();
  void process_modbus_server_frame(uint8_t address, uint8_t function_code, const std::vector<uint8_t> &data) override;
  void process_modbus_client_frame_(uint8_t address, uint8_t function_code, const std::vector<uint8_t> &data);
  void send_exception_(uint8_t address, uint8_t function_code, ModbusExceptionCode exception_code);
  void send_response_(uint8_t address, uint8_t function_code, std::vector<uint8_t> &&payload);
  void send_raw_(const std::vector<uint8_t> &payload);
  uint8_t expecting_peer_response_{0};
  std::vector<ModbusServerDevice *> devices_;
};

class ModbusClientDevice {
 public:
  ModbusClientDevice() = default;
  ModbusClientDevice(ModbusClientHub *parent, uint8_t address) : parent_(parent), address_(address) {}
  virtual ~ModbusClientDevice() { this->clear_tx_queue_for_device(); }
  void set_parent(ModbusClientHub *parent) { parent_ = parent; }
  void set_address(uint8_t address) { address_ = address; }
  virtual void on_modbus_data(const std::vector<uint8_t> &data) {}
  virtual void on_modbus_error(uint8_t function_code, uint8_t exception_code) {}
  virtual void on_modbus_sent() {}
  virtual void on_modbus_not_sent() {}
  virtual void on_modbus_no_response() {}
  void send(uint8_t function_code, uint16_t start_address, uint16_t number_of_entities, bool continuous = false);
  void send_pdu(const std::vector<uint8_t> &pdu, bool continuous = false);
  inline void clear_tx_queue_for_address(bool clear_sent = true) {
    this->parent_->clear_tx_queue_for_address(this->address_, clear_sent);
  }
  inline void clear_tx_queue_for_device() { this->parent_->clear_tx_queue_for_device(this); }

 protected:
  ModbusClientHub *parent_;
  uint8_t address_;
};

struct ModbusServerResponse {
  ModbusExceptionCode exception;
  std::vector<uint8_t> payload;
};

class ModbusServerDevice {
 public:
  ModbusServerDevice() = default;
  ModbusServerDevice(uint8_t address) : address_(address) {}
  void set_address(uint8_t address) { address_ = address; }
  uint8_t get_address() const { return address_; }
  virtual ModbusServerResponse on_modbus_read_registers(uint8_t function_code, uint16_t start_address,
                                                        uint16_t number_of_registers) = 0;
  virtual ModbusServerResponse on_modbus_write_registers(uint8_t function_code, const std::vector<uint8_t> &data) = 0;

 protected:
  uint8_t address_;
};

}  // namespace modbus
}  // namespace esphome

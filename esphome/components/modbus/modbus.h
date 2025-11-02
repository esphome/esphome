#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

#include "esphome/components/modbus/modbus_definitions.h"

#include <vector>
#include <queue>
#include <optional>

namespace esphome {
namespace modbus {

static const uint16_t MODBUS_TX_BUFFER_SIZE = 100;

class ModbusDevice;

class Modbus : public uart::UARTDevice, public Component {
 public:
  Modbus() = default;

  void setup() override;

  // TODO: Move this to server only.
  void register_device(ModbusDevice *device) { this->devices_.push_back(device); }

  float get_setup_priority() const override;
  virtual bool tx_blocked();
  virtual bool tx_buffer_empty() { return false; }  // TODO Remove this function from Modbus base class

  virtual void send(uint8_t address, uint8_t function_code, uint16_t start_address, uint16_t number_of_entities,
                    uint8_t payload_len = 0, const uint8_t *payload = nullptr) = 0;
  virtual void send_raw(const std::vector<uint8_t> &payload) = 0;
  void set_flow_control_pin(GPIOPin *flow_control_pin) { this->flow_control_pin_ = flow_control_pin; }

 protected:
  void receive_and_parse_modbus_bytes_();
  virtual void parse_modbus_byte_(uint8_t byte) = 0;
  bool parse_modbus_server_byte_(uint8_t byte);
  virtual void process_modbus_server_frame_(uint8_t address, uint8_t function_code,
                                            const std::vector<uint8_t> &data) = 0;
  void clear_rx_buffer_(const std::string &reason, bool warn = false);
  void send_frame_(const std::vector<uint8_t> &payload);
  static const std::vector<uint8_t> add_crc_to_payload_(const std::vector<uint8_t> &payload);

  uint32_t last_modbus_byte_{0};
  uint32_t last_send_{0};
  uint32_t last_send_tx_offset_{0};
  uint16_t frame_delay_ms_{5};
  uint16_t long_rx_buffer_delay_ms_{0};
  uint16_t turnaround_delay_ms_{0};  // This is only used by ModbusClient. Servers respond immediately.

  GPIOPin *flow_control_pin_{nullptr};

  std::vector<uint8_t> rx_buffer_;
  std::vector<ModbusDevice *> devices_;
};

class ModbusClient : public Modbus {
 public:
  ModbusClient() = default;
  void dump_config() override;
  void loop() override;
  void set_send_wait_time(uint16_t time_in_ms) { send_wait_time_ = time_in_ms; }
  void set_turnaround_time(uint16_t time_in_ms) { turnaround_delay_ms_ = time_in_ms; }
  bool tx_buffer_empty() override;
  bool tx_blocked() override;
  void send(uint8_t address, uint8_t function_code, uint16_t start_address, uint16_t number_of_entities,
            uint8_t payload_len = 0, const uint8_t *payload = nullptr) override;
  void send_raw(const std::vector<uint8_t> &payload) override;

 protected:
  void parse_modbus_byte_(uint8_t byte) override;
  void process_modbus_server_frame_(uint8_t address, uint8_t function_code, const std::vector<uint8_t> &data) override;
  void send_next_frame_();

  uint16_t send_wait_time_{250};
  uint8_t waiting_for_response_{0};

  // std::queue is appropriate here since we need a FIFO buffer, and we can't know ahead of time how many
  // requests will be queued. Each modbus component may queue multiple requests, and the sequence of scheduling
  // may change at run time.
  std::queue<std::vector<uint8_t>> tx_buffer_;
};

class ModbusServer : public Modbus {
 public:
  ModbusServer() = default;
  void dump_config() override;
  void loop() override;
  void send(uint8_t address, uint8_t function_code, uint16_t start_address, uint16_t number_of_entities,
            uint8_t payload_len = 0, const uint8_t *payload = nullptr) override;
  void send_raw(const std::vector<uint8_t> &payload) override;

 protected:
  void parse_modbus_byte_(uint8_t byte) override;
  bool parse_modbus_client_byte_(std::optional<uint8_t> byte);
  void process_modbus_server_frame_(uint8_t address, uint8_t function_code, const std::vector<uint8_t> &data) override;
  void process_modbus_client_frame_(uint8_t address, uint8_t function_code, const std::vector<uint8_t> &data);
  uint8_t expecting_peer_response_{0};
};

class ModbusDevice {
 public:
  void set_parent(Modbus *parent) { parent_ = parent; }
  void set_address(uint8_t address) { address_ = address; }
  virtual void on_modbus_data(const std::vector<uint8_t> &data){};
  virtual void on_modbus_error(uint8_t function_code, uint8_t exception_code) {}
  virtual void on_modbus_read_registers(uint8_t function_code, uint16_t start_address, uint16_t number_of_registers){};
  virtual void on_modbus_write_registers(uint8_t function_code, const std::vector<uint8_t> &data){};
  void send(uint8_t function, uint16_t start_address, uint16_t number_of_entities, uint8_t payload_len = 0,
            const uint8_t *payload = nullptr) {
    this->parent_->send(this->address_, function, start_address, number_of_entities, payload_len, payload);
  }
  void send_raw(const std::vector<uint8_t> &payload) { this->parent_->send_raw(payload); }
  void send_error(uint8_t function_code, ModbusExceptionCode exception_code) {
    std::vector<uint8_t> error_response;
    error_response.reserve(3);
    error_response.push_back(this->address_);
    error_response.push_back(function_code | FUNCTION_CODE_EXCEPTION_MASK);
    error_response.push_back(static_cast<uint8_t>(exception_code));
    this->send_raw(error_response);
  }
  // If more than one device is connected block sending a new command before a response is received
  // TODO: tx_buffer_empty is not implemented for ModbusServer
  bool ready_for_immediate_send() { return parent_->tx_buffer_empty() && !parent_->tx_blocked(); }

 protected:
  // TODO: Double check friends.
  friend ModbusClient;
  friend ModbusServer;

  Modbus *parent_;
  uint8_t address_;
};

}  // namespace modbus
}  // namespace esphome

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

#include <vector>
#include <deque>

namespace esphome {
namespace modbus {

static const uint16_t MODBUS_TX_BUFFER_SIZE = 100;

enum ModbusRole {
  CLIENT,
  SERVER,
};

class ModbusDevice;

class Modbus : public uart::UARTDevice, public Component {
 public:
  Modbus() = default;

  void setup() override;

  void loop() override;

  void dump_config() override;

  void register_device(ModbusDevice *device) { this->devices_.push_back(device); }

  float get_setup_priority() const override;
  bool tx_buffer_empty();
  bool tx_blocked();

  void send(uint8_t address, uint8_t function_code, uint16_t start_address, uint16_t number_of_entities,
            uint8_t payload_len = 0, const uint8_t *payload = nullptr);
  void send_raw(const std::vector<uint8_t> &payload);
  void set_role(ModbusRole role) { this->role = role; }
  void set_flow_control_pin(GPIOPin *flow_control_pin) { this->flow_control_pin_ = flow_control_pin; }
  void set_send_wait_time(uint16_t time_in_ms) { send_wait_time_ = time_in_ms; }
  void set_turnaround_time(uint16_t time_in_ms) { turnaround_delay_ms_ = time_in_ms; }
  void set_disable_crc(bool disable_crc) { disable_crc_ = disable_crc; }

  ModbusRole role;

 protected:
  GPIOPin *flow_control_pin_{nullptr};
  uint8_t waiting_for_response_{0};

  bool parse_modbus_byte_(uint8_t byte);
  void receive_and_parse_modbus_bytes_();
  uint16_t send_wait_time_{250};
  uint16_t frame_delay_ms_{5};
  uint16_t long_rx_buffer_delay_ms_{0};
  uint16_t turnaround_delay_ms_{100};
  bool disable_crc_;
  std::vector<uint8_t> rx_buffer_;
  void clear_rx_buffer_(const std::string &reason);
  uint32_t last_modbus_byte_{0};
  uint32_t last_send_{0};
  uint32_t last_send_tx_offset_{0};
  std::vector<ModbusDevice *> devices_;
  std::deque<std::vector<uint8_t>> tx_buffer_;
  void send_next_frame_();
};

class ModbusDevice {
 public:
  void set_parent(Modbus *parent) { parent_ = parent; }
  void set_address(uint8_t address) { address_ = address; }
  virtual void on_modbus_data(const std::vector<uint8_t> &data) = 0;
  virtual void on_modbus_error(uint8_t function_code, uint8_t exception_code) {}
  virtual void on_modbus_read_registers(uint8_t function_code, uint16_t start_address, uint16_t number_of_registers){};
  virtual void on_modbus_write_registers(uint8_t function_code, const std::vector<uint8_t> &data){};
  void send(uint8_t function, uint16_t start_address, uint16_t number_of_entities, uint8_t payload_len = 0,
            const uint8_t *payload = nullptr) {
    this->parent_->send(this->address_, function, start_address, number_of_entities, payload_len, payload);
  }
  void send_raw(const std::vector<uint8_t> &payload) { this->parent_->send_raw(payload); }
  void send_error(uint8_t function_code, uint8_t exception_code) {
    std::vector<uint8_t> error_response;
    error_response.reserve(3);
    error_response.push_back(this->address_);
    error_response.push_back(function_code | 0x80);
    error_response.push_back(exception_code);
    this->send_raw(error_response);
  }
  // If more than one device is connected block sending a new command before a response is received
  bool ready_for_immediate_send() { return parent_->tx_buffer_empty() && !parent_->tx_blocked(); }

 protected:
  friend Modbus;

  Modbus *parent_;
  uint8_t address_;
};

}  // namespace modbus
}  // namespace esphome

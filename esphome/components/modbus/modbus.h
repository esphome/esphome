#pragma once
#include <queue>
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace modbus {

class ModbusDevice;

using on_modbus_data_callback_t = std::function<void(uint8_t address, uint8_t function_code, uint8_t exception_code,
                                                     const std::vector<uint8_t> &data)>;

class Modbus : public uart::UARTDevice, public Component {
 public:
  Modbus() = default;

  void setup() override;

  void loop() override;

  void dump_config() override;

  void register_device(ModbusDevice *device) { this->devices_.push_back(device); }

  float get_setup_priority() const override;

  void send_direct(uint8_t address, uint8_t function_code, uint16_t start_address, uint16_t number_of_entities,
                   uint8_t payload_len = 0, const uint8_t *payload = nullptr);
  void send(uint8_t address, uint8_t function_code, uint16_t start_address, uint16_t number_of_entities,
            uint8_t payload_len = 0, const uint8_t *payload = nullptr, bool force_send = false);
  void send_raw(const std::vector<uint8_t> &payload, bool force_send = false);
  void set_flow_control_pin(GPIOPin *flow_control_pin) { this->flow_control_pin_ = flow_control_pin; }
  bool waiting_for_response{false};
  void set_send_wait_time(uint16_t time_in_ms) { send_wait_time_ = time_in_ms; }

 protected:
  bool dispatch_to_device_(uint8_t address, uint8_t function_code, uint8_t exception_code,
                           const std::vector<uint8_t> &data);
  bool parse_modbus_byte_(uint8_t byte);

  GPIOPin *flow_control_pin_{nullptr};
  uint16_t send_wait_time_{250};
  std::vector<uint8_t> rx_buffer_;
  std::queue<std::vector<uint8_t>> request_queue_;
  uint32_t last_modbus_byte_{0};
  uint32_t last_send_{0};
  std::vector<ModbusDevice *> devices_;
};

uint16_t crc16(const uint8_t *data, uint8_t len);

class ModbusDevice {
 public:
  void set_parent(Modbus *parent) { parent_ = parent; }
  void set_address(uint8_t address) { address_ = address; }
  void set_use_send_direct(bool use_send_direct) { use_send_direct_ = use_send_direct; }
  virtual void on_modbus_data(const std::vector<uint8_t> &data) = 0;
  virtual void on_modbus_error(uint8_t function_code, uint8_t exception_code) {}
  void send(uint8_t function, uint16_t start_address, uint16_t number_of_entities, uint8_t payload_len = 0,
            const uint8_t *payload = nullptr) {
    this->parent_->send(this->address_, function, start_address, number_of_entities, payload_len, payload,
                        use_send_direct_);
  }
  void send_direct(uint8_t function, uint16_t start_address, uint16_t number_of_entities, uint8_t payload_len = 0,
                   const uint8_t *payload = nullptr) {
    this->parent_->send(this->address_, function, start_address, number_of_entities, payload_len, payload, true);
  }
  void send_raw(const std::vector<uint8_t> &payload) { this->parent_->send_raw(payload, use_send_direct_); }
  // If more than one device is connected sending a new command before a response is received should be blocked
  bool waiting_for_response() { return parent_->waiting_for_response; }

 protected:
  friend Modbus;

  Modbus *parent_;
  uint8_t address_;
  bool use_send_direct_{false};
};

}  // namespace modbus
}  // namespace esphome

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace rs485 {

class RS485Device;

class RS485 : public uart::UARTDevice, public Component {
 public:
  RS485() = default;

  void loop() override;

  void dump_config() override;

  void register_device(RS485Device *device) { this->devices_.push_back(device); }

  float get_setup_priority() const override;

  void send(uint8_t address, uint8_t function, uint16_t start_address, uint16_t register_count);

 protected:
  bool parse_rs485_byte_(uint8_t byte);

  std::vector<uint8_t> rx_buffer_;
  uint32_t last_rs485_byte_{0};
  std::vector<RS485Device *> devices_;
};

class RS485Device {
 public:
  void set_parent(RS485 *parent) { parent_ = parent; }
  void set_start_code(uint8_t address) { start_code_ = start_code }
  void set_address(uint8_t address) { address_ = address; }
  virtual void on_rs485_data(const std::vector<uint8_t> &data) = 0;

  void send(uint8_t function, uint16_t start_address, uint16_t register_count) {
    this->parent_->send(this->address_, function, start_address, register_count);
  }

 protected:
  friend RS485;

  RS485 *parent_;
  uint8_t start_code_;
  uint8_t address_;
};

}  // namespace rs485
}  // namespace esphome

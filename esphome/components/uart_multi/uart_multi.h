#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace uart_multi {

class UARTMultiDevice;

class UARTMulti : public uart::UARTDevice, public Component {
 public:
  UARTMulti() = default;

  void loop() override;

  void dump_config() override;

  void register_device(UARTMultiDevice *device) { this->devices_.push_back(device); }

  float get_setup_priority() const override;

  void send(const std::vector<uint8_t> &data);

 protected:
  std::vector<UARTMultiDevice *> devices_;
};

class UARTMultiDevice {
 public:
  void set_parent(UARTMulti *parent) { parent_ = parent; }
  virtual void on_uart_multi_byte(uint8_t byte) = 0;

  void send(const std::vector<uint8_t> &data) { this->parent_->send(data); }

 protected:
  friend UARTMulti;

  UARTMulti *parent_;
};

}  // namespace uart_multi
}  // namespace esphome

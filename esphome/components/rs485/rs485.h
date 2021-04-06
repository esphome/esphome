#pragma once

#include <queue>
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

  void send(std::vector<uint8_t> &data);

 protected:
  void parse_rs485_frame_();

  std::vector<uint8_t> rx_buffer_;
  std::queue<std::vector<uint8_t>> tx_buffer_;
  bool ready_to_tx_{true};
  uint32_t last_rs485_byte_{0};
  std::vector<RS485Device *> devices_;
};

class RS485Device {
 public:
  void set_parent(RS485 *parent) { parent_ = parent; }
  virtual void on_rs485_data(const std::vector<uint8_t> &data) = 0;

  void send(std::vector<uint8_t> &data) { this->parent_->send(data); }

 protected:
  friend RS485;

  RS485 *parent_;
  std::vector<uint8_t *> header_;
};

}  // namespace rs485
}  // namespace esphome

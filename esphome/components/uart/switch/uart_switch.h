#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/switch/switch.h"

#include <vector>

namespace esphome {
namespace uart {

class UARTSwitch : public switch_::Switch, public UARTDevice, public Component {
 public:
  void loop() override;

  void set_data(const std::vector<uint8_t> &data) {
    this->data_ = data;
    this->single_state_ = true;
  }
  void set_data_on(const std::vector<uint8_t> &data) { this->data_on_ = data; }
  void set_data_off(const std::vector<uint8_t> &data) { this->data_off_ = data; }
  void set_send_every(uint32_t send_every) { this->send_every_ = send_every; }

  void dump_config() override;

 protected:
  void write_command_();
  void write_command_(bool state);
  void write_state(bool state) override;
  std::vector<uint8_t> data_;
  std::vector<uint8_t> data_on_;
  std::vector<uint8_t> data_off_;
  bool single_state_{false};
  uint32_t send_every_;
  uint32_t last_transmission_;
};

}  // namespace uart
}  // namespace esphome

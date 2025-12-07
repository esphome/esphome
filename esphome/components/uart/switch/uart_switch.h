#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/switch/switch.h"

#include <cinttypes>
#include <vector>

namespace esphome::uart {

class UARTSwitch : public switch_::Switch, public UARTDevice, public Component {
 public:
  void loop() override;

  void set_data_on(std::vector<uint8_t> &&data) { this->data_on_ = std::move(data); }
  void set_data_on(std::initializer_list<uint8_t> data) { this->data_on_ = std::vector<uint8_t>(data); }
  void set_data_off(std::vector<uint8_t> &&data) { this->data_off_ = std::move(data); }
  void set_data_off(std::initializer_list<uint8_t> data) { this->data_off_ = std::vector<uint8_t>(data); }
  void set_send_every(uint32_t send_every) { this->send_every_ = send_every; }
  void set_single_state(bool single) { this->single_state_ = single; }

  void dump_config() override;

 protected:
  void write_command_(bool state);
  void write_state(bool state) override;
  std::vector<uint8_t> data_on_;
  std::vector<uint8_t> data_off_;
  bool single_state_{false};
  uint32_t send_every_;
  uint32_t last_transmission_;
};

}  // namespace esphome::uart

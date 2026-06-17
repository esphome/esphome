#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/button/button.h"

#include <vector>

namespace esphome::uart {

class UARTButton : public button::Button, public UARTDevice, public Component {
 public:
  void set_data(std::vector<uint8_t> &&data) { this->data_ = std::move(data); }
  void set_data(std::initializer_list<uint8_t> data) { this->data_ = std::vector<uint8_t>(data); }

  void dump_config() override;

 protected:
  void press_action() override;
  std::vector<uint8_t> data_;
};

}  // namespace esphome::uart

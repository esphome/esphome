#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/button/button.h"

#include <vector>

namespace esphome {
namespace uart {

class UARTButton : public button::Button, public UARTDevice, public Component {
 public:
  void set_data(const std::vector<uint8_t> &data) { this->data_ = data; }

  void dump_config() override;

 protected:
  void press_action() override;
  std::vector<uint8_t> data_;
};

}  // namespace uart
}  // namespace esphome

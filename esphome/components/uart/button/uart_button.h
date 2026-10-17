#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/button/button.h"

namespace esphome::uart {

class UARTButton final : public button::Button, public UARTDevice, public Component {
 public:
  void set_data(const uint8_t *data, uint16_t len) {
    this->data_ = data;
    this->data_len_ = len;
  }

  void dump_config() override;

 protected:
  void press_action() override;
  const uint8_t *data_{nullptr};
  uint16_t data_len_{0};
};

}  // namespace esphome::uart

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/switch/switch.h"

#include <cinttypes>

namespace esphome::uart {

class UARTSwitch final : public switch_::Switch, public UARTDevice, public Component {
 public:
  void loop() override;

  void set_data_on(const uint8_t *data, uint16_t len) {
    this->data_on_ = data;
    this->data_on_len_ = len;
  }
  void set_data_off(const uint8_t *data, uint16_t len) {
    this->data_off_ = data;
    this->data_off_len_ = len;
  }
  void set_send_every(uint32_t send_every) { this->send_every_ = send_every; }
  void set_single_state(bool single) { this->single_state_ = single; }

  void dump_config() override;

 protected:
  void write_command_(bool state);
  void write_state(bool state) override;
  const uint8_t *data_on_{nullptr};
  const uint8_t *data_off_{nullptr};
  uint16_t data_on_len_{0};
  uint16_t data_off_len_{0};
  bool single_state_{false};
  uint32_t send_every_;
  uint32_t last_transmission_;
};

}  // namespace esphome::uart

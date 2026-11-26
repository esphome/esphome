#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome::uart_tx_break_test {

// Holds TX low and releases it at setup, so the build test compiles set_tx_break() of the bus
class UARTTXBreakTest : public Component, public uart::UARTDevice {
 public:
  void setup() override {
    this->parent_->set_tx_break(true);
    this->parent_->set_tx_break(false);
  }
};

}  // namespace esphome::uart_tx_break_test

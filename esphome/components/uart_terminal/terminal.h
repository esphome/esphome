#pragma once

#include "esphome/components/text_display/text_display.h"
#include "esphome/components/display/display.h"
#include "esphome/components/uart/uart.h"

namespace esphome::uart_terminal {

class Terminal;

using terminal_writer_t = display::DisplayWriter<Terminal>;

class Terminal : public text_display::TextDisplay, public uart::UARTDevice {
 public:
  void set_writer(terminal_writer_t &&writer) { this->writer_ = std::move(writer); }
  void display() override;
  void setup() override;

 protected:
  void call_writer() override { this->writer_(*this); }
  terminal_writer_t writer_;
};
}  // namespace esphome::uart_terminal

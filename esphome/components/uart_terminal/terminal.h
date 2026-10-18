#pragma once

#include "esphome/components/display/display.h"
#include "esphome/components/text_display/text_display.h"
#include "esphome/components/uart/uart.h"

namespace esphome::uart_terminal {

class Terminal;

using terminal_writer_t = display::DisplayWriter<Terminal>;

/// A text display drawn on an ANSI terminal attached to a UART
class Terminal final : public text_display::TextDisplay, public uart::UARTDevice {
 public:
  void set_writer(terminal_writer_t &&writer) { this->writer_ = std::move(writer); }
  void update() override;
  void loop() override;
  void display() override;
  void dump_config() override;

 protected:
  void call_writer() override { this->writer_(*this); }
  terminal_writer_t writer_;
  uint32_t last_frame_hash_{0};
  uint32_t last_write_ms_{0};
  // The frame goes out from loop() in pieces the UART takes without waiting; the buffer stays as it is until
  // the last piece is sent
  uint8_t tx_row_{0};
  uint8_t tx_col_{0};
  uint8_t updates_until_redraw_{0};  // 0 forces the next frame out with a cleared screen
  bool sending_{false};
  bool tx_full_{false};
  bool tx_prefix_pending_{false};
  bool tx_crlf_pending_{false};
};

}  // namespace esphome::uart_terminal

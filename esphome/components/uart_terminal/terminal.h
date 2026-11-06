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
  bool write_sequence_(const uint8_t *seq, size_t len, size_t &room);
  terminal_writer_t writer_;
  uint32_t last_frame_hash_{0};
  uint32_t last_write_ms_{0};
  // The frame goes out from loop() in pieces the UART takes without waiting; the buffer stays as it is until
  // the last piece is sent
  uint8_t tx_row_{0};
  uint8_t tx_col_{0};
  uint8_t updates_until_redraw_{0};  // 0 forces the next frame out with a cleared screen
  uint8_t tx_seq_pos_ : 3 {0};       // bytes of the current escape sequence or CRLF already sent
  bool sending_ : 1 {false};
  bool update_pending_ : 1 {false};  // an update arrived while a frame was being sent
  bool tx_full_ : 1 {false};
  bool tx_prefix_pending_ : 1 {false};
  bool tx_crlf_pending_ : 1 {false};
};

}  // namespace esphome::uart_terminal

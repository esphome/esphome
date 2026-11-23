#pragma once

#ifdef USE_ESP32
#include "esphome/core/helpers.h"
#include <driver/uart.h>

namespace esphome::logger {

#ifdef USE_LOGGER_UART_SELECTION_USB_SERIAL_JTAG
void usb_serial_jtag_write_line(const char *msg, size_t len);
#endif

inline void HOT Logger::write_msg_(const char *msg, uint16_t len) {
#if defined(USE_LOGGER_UART_SELECTION_USB_SERIAL_JTAG)
  // Through the driver in whole lines: ESP-IDF's stdout VFS hands it one character per call.
  usb_serial_jtag_write_line(msg, len);
#elif defined(USE_LOGGER_UART_SELECTION_USB_CDC)
  // USB CDC - single write including newline, through stdout and VFS to the USB console
  fwrite(msg, 1, len, stdout);
#else
  // Regular UART - single write including newline (already in buffer)
  uart_write_bytes(this->get_uart_num(), msg, len);
#endif
}

}  // namespace esphome::logger

#endif

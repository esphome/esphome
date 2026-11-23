#pragma once

#ifdef USE_ESP32
#include "esphome/core/helpers.h"
#include <driver/uart.h>

namespace esphome::logger {

#if defined(USE_LOGGER_UART_SELECTION_USB_SERIAL_JTAG)
void usb_serial_jtag_write_line(const char *msg, size_t len);
#elif defined(USE_LOGGER_UART_SELECTION_USB_CDC)
void usb_cdc_write_line(const char *msg, size_t len);
#endif

inline void HOT Logger::write_msg_(const char *msg, uint16_t len) {
  // One write per line, newline included. The USB consoles are written through their drivers, not stdout:
  // ESP-IDF's VFS hands those drivers one character at a time.
#if defined(USE_LOGGER_UART_SELECTION_USB_SERIAL_JTAG)
  usb_serial_jtag_write_line(msg, len);
#elif defined(USE_LOGGER_UART_SELECTION_USB_CDC)
  usb_cdc_write_line(msg, len);
#else
  uart_write_bytes(this->get_uart_num(), msg, len);
#endif
}

}  // namespace esphome::logger

#endif

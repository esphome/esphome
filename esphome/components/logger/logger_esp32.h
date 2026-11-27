#pragma once

#ifdef USE_ESP32
#include "esphome/core/helpers.h"
#include <driver/uart.h>

namespace esphome::logger {

#ifdef USE_LOGGER_UART_SELECTION_USB_SERIAL_JTAG
void write_usb_serial_jtag_line(const char *msg, size_t len);
#endif

inline void HOT Logger::write_msg_(const char *msg, uint16_t len) {
#if defined(USE_LOGGER_UART_SELECTION_USB_CDC) || defined(USE_LOGGER_UART_SELECTION_USB_SERIAL_JTAG)
  // USB CDC/JTAG - single write including newline (already in buffer)
  // Use fwrite to stdout which goes through VFS to USB console
  //
  // Note: These defines indicate the user's YAML configuration choice (hardware_uart: USB_CDC/USB_SERIAL_JTAG).
  // They are ONLY defined when the user explicitly selects USB as the logger output in their config.
  // This is compile-time selection, not runtime detection - if USB is configured, it's always used.
  // There is no fallback to regular UART if "USB isn't connected" - that's the user's responsibility
  // to configure correctly for their hardware. This approach eliminates runtime overhead.
#ifdef USE_LOGGER_UART_SELECTION_USB_SERIAL_JTAG
  // Not through stdout: its VFS hands the driver one character per call
  write_usb_serial_jtag_line(msg, len);
#else
  fwrite(msg, 1, len, stdout);
#endif
#else
  // Regular UART - single write including newline (already in buffer)
  uart_write_bytes(this->get_uart_num(), msg, len);
#endif
}

}  // namespace esphome::logger

#endif
